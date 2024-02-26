void CPU_Info::ipi_reschedule()
{
    send_ipi_fixed(ipi_reschedule_int_vec, lapic_id);
}