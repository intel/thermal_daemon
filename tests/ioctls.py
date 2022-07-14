import struct

# Length of unsigned long
ptr_size = len(struct.pack('l', 0)) 

IOC_WRITE = 1
IOC_READ = 2

ACPI_THERMAL_MAGIC = ord('s')

def _IOC(dir_, type_, nr, size):
    _IOC_NRBITS     = 8
    _IOC_TYPEBITS   = 8

    _IOC_SIZEBITS  = 14
    _IOC_DIRBITS   = 2

    _IOC_NRSHIFT    = 0
    _IOC_TYPESHIFT  = (_IOC_NRSHIFT+_IOC_NRBITS)
    _IOC_SIZESHIFT  = (_IOC_TYPESHIFT+_IOC_TYPEBITS)
    _IOC_DIRSHIFT   = (_IOC_SIZESHIFT+_IOC_SIZEBITS)

    return (((dir_)  << _IOC_DIRSHIFT) | \
            ((type_) << _IOC_TYPESHIFT) | \
            ((nr)   << _IOC_NRSHIFT) | \
            ((size) << _IOC_SIZESHIFT))

_IOR = lambda t, n, s: _IOC(IOC_READ, t, n, s)

ACPI_THERMAL_GET_TRT_LEN       = _IOR(ACPI_THERMAL_MAGIC, 1, ptr_size)
ACPI_THERMAL_GET_ART_LEN       = _IOR(ACPI_THERMAL_MAGIC, 2, ptr_size)
ACPI_THERMAL_GET_TRT_COUNT     = _IOR(ACPI_THERMAL_MAGIC, 3, ptr_size)
ACPI_THERMAL_GET_ART_COUNT     = _IOR(ACPI_THERMAL_MAGIC, 4, ptr_size)

ACPI_THERMAL_GET_TRT    = _IOR(ACPI_THERMAL_MAGIC, 5, ptr_size)
ACPI_THERMAL_GET_ART    = _IOR(ACPI_THERMAL_MAGIC, 6, ptr_size)

