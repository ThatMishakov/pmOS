use super::error::Error;
use bytemuck::Pod;
use bytemuck::Zeroable;
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::CStr;

#[derive(Debug)]
pub enum ObjectProperty {
    String(Box<str>),
    Integer(u64),
    List(BTreeSet<Box<str>>),
}

pub enum ObjectPropertyRef<'a> {
    String(&'a str),
    Integer(u64),
    List(&'a BTreeSet<Box<str>>),
}

#[derive(Debug)]
pub struct ObjectProperties(BTreeMap<Box<str>, ObjectProperty>);

impl ObjectProperties {
    pub fn get_property(&self, property_name: &str) -> Option<ObjectPropertyRef> {
        self.0.get(property_name).map(|p| match p {
            ObjectProperty::String(t) => ObjectPropertyRef::String(t),
            ObjectProperty::Integer(i) => ObjectPropertyRef::Integer(*i),
            ObjectProperty::List(l) => ObjectPropertyRef::List(l),
        })
    }

    pub fn new() -> ObjectProperties {
        ObjectProperties(BTreeMap::new())
    }
}

pub struct PMBusObject {
    pub properties: ObjectProperties,
    pub name: Box<str>,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Zeroable, Pod)]
pub struct IPCObjectPropertyHdr {
    pub length: u16,    // total record length incl padding
    pub type_: u8,      // PROPERTY_TYPE_*
    pub data_start: u8, // offset from start of record to value bytes
}

pub const PROPERTY_TYPE_STRING: u8 = 0x01;
pub const PROPERTY_TYPE_INTEGER: u8 = 0x02;
pub const PROPERTY_TYPE_LIST: u8 = 0x03;

#[inline]
fn push_pod<T: Pod>(out: &mut Vec<u8>, v: &T) {
    out.extend_from_slice(bytemuck::bytes_of(v));
}

#[inline]
fn align_up(x: usize, alignment: usize) -> usize {
    debug_assert!(alignment.is_power_of_two());
    (x + (alignment - 1)) & !(alignment - 1)
}

fn push_property_string(out: &mut Vec<u8>, name: &str, value: &str) {
    let hdr_len = core::mem::size_of::<IPCObjectPropertyHdr>();
    let name_len = name.len();
    let value_len = value.len();

    let name_hdr_len = hdr_len + name_len + 1;
    let total_size = name_hdr_len + value_len + 1;
    let size_aligned = align_up(total_size, 8);

    let hdr = IPCObjectPropertyHdr {
        length: size_aligned as u16,
        type_: PROPERTY_TYPE_STRING,
        data_start: name_hdr_len as u8,
    };

    push_pod(out, &hdr);
    out.extend_from_slice(name.as_bytes());
    out.push(0);

    out.extend_from_slice(value.as_bytes());
    out.push(0);

    out.resize(size_aligned, 0);
}

fn push_property_list(
    out: &mut Vec<u8>,
    name: &str,
    items: impl IntoIterator<Item = impl AsRef<str>>,
) {
    let hdr_len = core::mem::size_of::<IPCObjectPropertyHdr>();
    let name_len = name.len();

    let name_hdr_len = hdr_len + name_len + 1;

    let mut list_bytes = Vec::new();
    for s in items {
        let s = s.as_ref();
        list_bytes.extend_from_slice(s.as_bytes());
        list_bytes.push(0);
    }

    let total_size = name_hdr_len + list_bytes.len();
    let size_aligned = align_up(total_size, 8);

    let hdr = IPCObjectPropertyHdr {
        length: total_size as u16,
        type_: PROPERTY_TYPE_LIST,
        data_start: name_hdr_len as u8,
    };

    push_pod(out, &hdr);
    out.extend_from_slice(name.as_bytes());
    out.push(0);
    out.extend_from_slice(&list_bytes);

    out.resize(size_aligned, 0);
}

fn push_property_u64(out: &mut Vec<u8>, name: &str, value: u64) {
    let hdr_len = core::mem::size_of::<IPCObjectPropertyHdr>();
    let name_len = name.len();

    let name_hdr_len = hdr_len + name_len + 1;
    let data_offset = align_up(name_hdr_len, 8);
    let total_size = data_offset + 8;

    let hdr = IPCObjectPropertyHdr {
        length: total_size as u16,
        type_: PROPERTY_TYPE_INTEGER,
        data_start: data_offset as u8,
    };

    push_pod(out, &hdr);
    out.extend_from_slice(name.as_bytes());
    out.push(0);

    out.resize(data_offset, 0);
    out.extend_from_slice(&value.to_ne_bytes());
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Zeroable, Pod)]
pub struct IPCBusObjectHdr {
    pub size: u32,
    pub name_length: u16,
    pub properties_offset: u16,
}

impl PMBusObject {
    pub fn new(name: Box<str>) -> PMBusObject {
        PMBusObject {
            properties: ObjectProperties::new(),
            name: name,
        }
    }

    pub fn get_property(&self, property_name: &str) -> Option<ObjectPropertyRef> {
        self.properties.get_property(property_name)
    }

    pub fn deserialize(data: &[u8]) -> Result<PMBusObject, Error> {
        if data.len() < 8 {
            return Err(Error::from_errno(libc::EINVAL));
        }

        let size = u32::from_ne_bytes(
            data[0..4]
                .try_into()
                .map_err(|_| Error::from_errno(libc::EINVAL))?,
        );
        let name_length = u16::from_ne_bytes(
            data[4..6]
                .try_into()
                .map_err(|_| Error::from_errno(libc::EINVAL))?,
        ) as usize;
        let mut properties_offset = u16::from_ne_bytes(
            data[6..8]
                .try_into()
                .map_err(|_| Error::from_errno(libc::EINVAL))?,
        ) as usize;

        if name_length + 8 > properties_offset {
            return Err(Error::from_errno(libc::EINVAL));
        }

        let name_data = data
            .get(8..8 + name_length)
            .ok_or(Error::from_errno(libc::EINVAL))?;
        let name = str::from_utf8(name_data).map_err(|_| Error::from_errno(libc::EINVAL))?;

        let mut properties = BTreeMap::new();
        while properties_offset < size as usize {
            let length = u16::from_ne_bytes(
                data.get(properties_offset..properties_offset + 2)
                    .ok_or(Error::from_errno(libc::EINVAL))?
                    .try_into()
                    .map_err(|_| Error::from_errno(libc::EINVAL))?,
            ) as usize;
            let property_type = *data
                .get(properties_offset + 2)
                .ok_or(Error::from_errno(libc::EINVAL))?;
            let data_start = *data
                .get(properties_offset + 3)
                .ok_or(Error::from_errno(libc::EINVAL))? as usize
                + properties_offset;
            let property_name = data
                .get(properties_offset + 4..data_start)
                .ok_or(Error::from_errno(libc::EINVAL))?;
            let property_name = CStr::from_bytes_until_nul(property_name)
                .map_err(|_| Error::from_errno(libc::EINVAL))?;
            let property_name = property_name
                .to_str()
                .map_err(|_| Error::from_errno(libc::EINVAL))?;

            let property = match property_type {
                0x01 => {
                    let data_end = properties_offset + length;
                    let array = data
                        .get(data_start..data_end)
                        .ok_or(Error::from_errno(libc::EINVAL))?;
                    let value = CStr::from_bytes_until_nul(array)
                        .map_err(|_| Error::from_errno(libc::EINVAL))?;
                    let value = value
                        .to_str()
                        .map_err(|_| Error::from_errno(libc::EINVAL))?;
                    ObjectProperty::String(Box::from(value))
                }
                0x02 => {
                    let integer_value = u64::from_ne_bytes(
                        data.get(data_start..data_start + 8)
                            .ok_or(Error::from_errno(libc::EINVAL))?
                            .try_into()
                            .map_err(|_| Error::from_errno(libc::EINVAL))?,
                    );
                    ObjectProperty::Integer(integer_value)
                }
                0x03 => {
                    let data_end = properties_offset + length;
                    let mut array = data
                        .get(data_start..data_end)
                        .ok_or(Error::from_errno(libc::EINVAL))?;
                    let mut set: BTreeSet<Box<str>> = BTreeSet::new();

                    while !array.is_empty() {
                        let value = CStr::from_bytes_until_nul(array)
                            .map_err(|_| Error::from_errno(libc::EINVAL))?;
                        let value = value
                            .to_str()
                            .map_err(|_| Error::from_errno(libc::EINVAL))?;
                        let len = value.len();
                        set.insert(Box::from(value));

                        array = &array[len..];
                        let skip = array.iter().position(|&x| x != 0).unwrap_or(array.len());
                        array = &array[skip..];
                    }

                    ObjectProperty::List(set)
                }
                _ => return Err(Error::from_errno(libc::EINVAL)),
            };

            properties.insert(Box::from(property_name), property);
            properties_offset += length;
        }

        Ok(PMBusObject {
            properties: ObjectProperties(properties),
            name: Box::from(name),
        })
    }
}

pub fn pmbus_object_serialize(name: &str, properties: &ObjectProperties) -> Vec<u8> {
    let hdr_len = core::mem::size_of::<IPCBusObjectHdr>();

    let name_len = name.len();
    let properties_offset = align_up(hdr_len + name_len, 8);
    let properties_offset_u16 = properties_offset as u16;

    let mut props = Vec::new();
    for i in &properties.0 {
        match i.1 {
            ObjectProperty::String(s) => push_property_string(&mut props, i.0.as_ref(), s.as_ref()),
            ObjectProperty::Integer(integ) => push_property_u64(&mut props, i.0.as_ref(), *integ),
            ObjectProperty::List(l) => push_property_list(&mut props, i.0.as_ref(), l),
        }
    }

    let total_size = properties_offset + props.len();
    let hdr = IPCBusObjectHdr {
        size: total_size as u32,
        name_length: name.len() as u16, // matches your C: strlen(name)
        properties_offset: properties_offset_u16,
    };

    let mut out = Vec::with_capacity(total_size);
    push_pod(&mut out, &hdr);

    out.extend_from_slice(name.as_bytes());
    out.push(0);

    out.resize(properties_offset, 0);

    out.extend_from_slice(&props);
    debug_assert_eq!(out.len(), total_size);

    out
}

impl std::fmt::Debug for PMBusObject {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("PMBusObject")
            .field("name", &self.name)
            .field("properties", &self.properties)
            .finish()
    }
}

#[derive(Debug)]
pub enum AnyFilter {
    EqualsFilter(EqualsFilter),
    Conjunction(Conjunction),
    Disjunction(Disjunction),
}

#[derive(Debug)]
pub struct Conjunction {
    pub operands: Vec<AnyFilter>,
}

#[derive(Debug)]
pub struct Disjunction {
    pub operands: Vec<AnyFilter>,
}

#[derive(Debug)]
pub struct EqualsFilter {
    pub name: Box<str>,
    pub property: Box<str>,
}

impl From<EqualsFilter> for AnyFilter {
    fn from(filter: EqualsFilter) -> Self {
        AnyFilter::EqualsFilter(filter)
    }
}
impl From<Conjunction> for AnyFilter {
    fn from(filter: Conjunction) -> Self {
        AnyFilter::Conjunction(filter)
    }
}
impl From<Disjunction> for AnyFilter {
    fn from(filter: Disjunction) -> Self {
        AnyFilter::Disjunction(filter)
    }
}

const FILTER_EQUALS_TYPE: u32 = 1;
const FILTER_CONJUNCTION_TYPE: u32 = 2;
const FILTER_DISJUNCTION_TYPE: u32 = 3;

fn match_number(s: &str, i: u64) -> bool {
    s.strip_prefix("0x")
        .map(|s| u64::from_str_radix(s, 16))
        .or_else(|| s.strip_prefix("0o").map(|s| u64::from_str_radix(s, 8)))
        .or_else(|| Some(s.parse::<u64>()))
        .and_then(Result::ok)
        .is_some_and(|v| v == i)
}

impl AnyFilter {
    pub fn matches(&self, object: &ObjectProperties) -> bool {
        match self {
            AnyFilter::EqualsFilter(filter) => match object.get_property(filter.name.as_ref()) {
                Some(ObjectPropertyRef::String(s)) => *s == *filter.property,
                Some(ObjectPropertyRef::Integer(i)) => match_number(&filter.property, i),
                Some(ObjectPropertyRef::List(l)) => l.contains(&filter.property),
                None => false,
            },
            AnyFilter::Conjunction(conjunction) => {
                conjunction.operands.iter().all(|f| f.matches(object))
            }
            AnyFilter::Disjunction(disjunction) => {
                disjunction.operands.iter().any(|f| f.matches(object))
            }
        }
    }

    pub fn deserialize(data: &[u8]) -> Result<(AnyFilter, u32 /* size */), Error> {
        if data.len() < 8 || data.len() % 8 != 0 {
            return Err(Error::from_errno(libc::EINVAL));
        }

        let filter_type = u32::from_ne_bytes(data[0..4].try_into().unwrap());
        let len = u32::from_ne_bytes(data[4..8].try_into().unwrap());

        match filter_type {
            FILTER_EQUALS_TYPE => {
                // This has potential panicks with bogus value_length overflows, and so on...
                let key_length = u32::from_ne_bytes(
                    data.get(8..12)
                        .ok_or_else(|| Error::from_errno(libc::EINVAL))?
                        .try_into()
                        .unwrap(),
                ) as usize;
                let value_length = u32::from_ne_bytes(
                    data.get(12..16)
                        .ok_or(Error::from_errno(libc::EINVAL))?
                        .try_into()
                        .map_err(|_| Error::from_errno(libc::EINVAL))?,
                ) as usize;
                let key = str::from_utf8(
                    data.get(16..16 + key_length)
                        .ok_or(Error::from_errno(libc::EINVAL))?,
                )
                .map_err(|_| Error::from_errno(libc::EINVAL))?;
                let value = str::from_utf8(
                    data.get(16 + key_length + 1..16 + key_length + 1 + value_length)
                        .ok_or(Error::from_errno(libc::EINVAL))?,
                )
                .map_err(|_| Error::from_errno(libc::EINVAL))?;
                Ok((
                    AnyFilter::EqualsFilter(EqualsFilter {
                        name: Box::from(key),
                        property: Box::from(value),
                    }),
                    len,
                ))
            }
            FILTER_CONJUNCTION_TYPE | FILTER_DISJUNCTION_TYPE => {
                let mut pos = 8u32; // From header
                let mut filters = vec![];
                while pos < len {
                    let (filter, flen) = AnyFilter::deserialize(&data[pos as usize..])?;
                    if pos
                        .checked_add(flen)
                        .ok_or(Error::from_errno(libc::EINVAL))?
                        > len
                    {
                        return Err(Error::from_errno(libc::EINVAL));
                    }

                    filters.push(filter);
                    pos += flen;
                }

                if filters.len() == 0 {
                    Err(Error::from_errno(libc::EINVAL))
                } else if filter_type == FILTER_CONJUNCTION_TYPE {
                    Ok((
                        AnyFilter::Conjunction(Conjunction { operands: filters }),
                        len,
                    ))
                } else {
                    Ok((
                        AnyFilter::Disjunction(Disjunction { operands: filters }),
                        len,
                    ))
                }
            }
            _ => Err(Error::from_errno(libc::ENOSYS)),
        }
    }
}
