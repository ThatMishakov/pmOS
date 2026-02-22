use super::error::Error;
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

pub type ObjectProperties = BTreeMap<Box<str>, ObjectProperty>;

pub struct PMBusObject {
    pub properties: ObjectProperties,
    pub name: Box<str>,
    pub handle_port: u64,
}

impl PMBusObject {
    pub fn new(name: Box<str>) -> PMBusObject {
        PMBusObject {
            properties: BTreeMap::new(),
            name: name,
            handle_port: 0,
        }
    }

    pub fn get_property(&self, property_name: &str) -> Option<ObjectPropertyRef> {
        self.properties.get(property_name).map(|p| match p {
            ObjectProperty::String(t) => ObjectPropertyRef::String(t),
            ObjectProperty::Integer(i) => ObjectPropertyRef::Integer(*i),
            ObjectProperty::List(l) => ObjectPropertyRef::List(l),
        })
    }

    pub fn deserialize(data: &[u8]) -> Result<(PMBusObject, u64), Error> {
        if data.len() < 24 {
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
        let handle_port = u64::from_ne_bytes(
            data[8..16]
                .try_into()
                .map_err(|_| Error::from_errno(libc::EINVAL))?,
        );
        let task_group = u64::from_ne_bytes(
            data[16..24]
                .try_into()
                .map_err(|_| Error::from_errno(libc::EINVAL))?,
        );

        if name_length + 24 > properties_offset {
            return Err(Error::from_errno(libc::EINVAL));
        }

        let name_data = data
            .get(24..24 + name_length)
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

        Ok((
            PMBusObject {
                properties,
                name: Box::from(name),
                handle_port: handle_port,
            },
            task_group,
        ))
    }
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

impl AnyFilter {
    pub fn matches(&self, object: &PMBusObject) -> bool {
        match self {
            AnyFilter::EqualsFilter(filter) => match object.get_property(filter.name.as_ref()) {
                Some(ObjectPropertyRef::String(s)) => *s == *filter.property,
                Some(ObjectPropertyRef::Integer(i)) => {
                    filter.property.parse().ok().is_some_and(|v: u64| v == i)
                }
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
                    if pos.checked_add(flen).ok_or(Error::from_errno(libc::EINVAL))? > len {
                        return Err(Error::from_errno(libc::EINVAL));
                    }

                    filters.push(filter);
                    pos += flen;
                }

                if filters.len() == 0 {
                    Err(Error::from_errno(libc::EINVAL))
                } else if filter_type == FILTER_CONJUNCTION_TYPE {
                    Ok((
                        AnyFilter::Conjunction(Conjunction{
                            operands: filters,
                        }),
                        len
                    ))
                } else {
                    Ok((
                        AnyFilter::Disjunction(Disjunction{
                            operands: filters,
                        }),
                        len
                    ))
                }
            }
            _ => Err(Error::from_errno(libc::ENOSYS)),
        }
    }
}
