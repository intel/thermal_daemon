"""
   This simple utility reads data_vault in sysfs
   and generate data_valut.bin
   The path for input is for Panther Lake, but
   can be modified for others.
"""
def dup_sysfs_data_binary(input_file, output_file):
    try:
        with open(input_file, 'rb') as f:
            content = f.read()
            with open(output_file, 'wb') as fw:
                fw.write(content)
        return content
    except FileNotFoundError:
        print(f"Error: File not found at {filepath}")
        return None
    except PermissionError:
        print(f"Error: Permission denied when accessing {filepath}")
        return None
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        return None

input_path = '/sys/bus/platform/devices/INTC10D4:00/data_vault'
output_path = 'data_vault.bin'
binary_data = dup_sysfs_data_binary(input_path, output_path)

if binary_data is not None:
    print(f"Binary content of '{input_path}':")
    print(binary_data)
