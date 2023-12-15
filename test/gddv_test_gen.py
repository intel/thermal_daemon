# Read gddv_test.json and generate gddv file
import json
import sys

data = b""
file_name = ""


def create_uintu64(value):
    global data
    header = b"\x04\x00\x00\x00"
    data = data + header + int(value).to_bytes(8, "little")


def create_string(value):
    global data
    header = b"\x08\x00\x00\x00"
    byte_array = value.encode()
    byte_array = byte_array + b'\x00'
    length = len(value) + 1
    length = length.to_bytes(8, "little")
    data = data + header + length + byte_array


def create_key(key, val_array):
    global data
    key_len = len(key) + 1
    key_flags = b"\x00\x00\x00\x00"
    key_length = int(key_len).to_bytes(4, "little")
    key_array = key.encode()
    key_array = key_array + b"\x00"

    val_type = b"\x00\x00\x00\x00"
    val_length = len(val_array).to_bytes(4, "little")

    data = data + key_flags + key_length + key_array + val_type + val_length + val_array


def create_header():
    global data
    data = b"\xE5\x1F\x0C\x00\x00\x00\x00\x01\x00\x00\x00\x00"


def create_psvt(binary_file):
    global data
    data = b""

    with open(file_name, "r") as gddv_test_file:
        gddv_data = json.load(gddv_test_file)
        print(json.dumps(gddv_data, indent=4))

        psvts = {}
        for key, value in gddv_data.items():
            if key == "PSVT":
                for index in range(len(gddv_data["PSVT"])):
                    psvts[index] = gddv_data["PSVT"][index]
                    print("index", index)
                    print("    name", psvts[index]["name"])
                    key_name = "/shared/tables/psvt/" + psvts[index]["name"]

                    table = psvts[index]["table"]
                    create_uintu64("2")
                    for i in range(len(table)):
                        print("table_index", i)
                        # create_uintu64("2")
                        print("    source:", table[i]["source"])
                        create_string(table[i]["source"])
                        print("    target:", table[i]["target"])
                        create_string(table[i]["target"])
                        print("    priority:", table[i]["priority"])
                        create_uintu64(table[i]["priority"])
                        print("    sample_peri od:", table[i]["sample_period"])
                        create_uintu64(table[i]["sample_period"])
                        print("    temperature:", table[i]["temperature"])
                        temp = int(table[i]["temperature"])
                        temp = temp * 10 + 2727
                        create_uintu64(str(temp))
                        print("    domain:", table[i]["domain"])
                        create_uintu64(table[i]["domain"])
                        print("    control_knob:", table[i]["control_knob"])
                        create_uintu64(table[i]["control_knob"])
                        if table[i]["limit"] == "MIN" or table[i]["limit"] == "MAX":
                            create_string(table[i]["limit"])
                        else:
                            create_uintu64(table[i]["limit"])
                        print("    step_size:", table[i]["step_size"])
                        create_uintu64(table[i]["step_size"])
                        print("    limit_coeff:", table[i]["limit_coeff"])
                        create_uintu64(table[i]["limit_coeff"])
                        print("    unlimit_coeff:", table[i]["unlimit_coeff"])
                        create_uintu64(table[i]["unlimit_coeff"])
                        data = (
                            data + b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                        )
                        print("(", data, ")")

                    print("[", data, "]")
                    psvt_entry = data
                    data = b""
                    create_key(key_name, psvt_entry)
                    binary_file.write(data)
                    data = b""


def parse_psvt_json(binary_file):
    print(binary_file)
    create_psvt(binary_file)


def create_gddv():
    print("create gddv")
    with open("gddv.bin", "wb") as binary_file:
        create_header()
        binary_file.write(data)
        parse_psvt_json(binary_file)


# main entry point
# Get the config file name from command line
print("Number of arguments:", len(sys.argv), "arguments.")
print("Argument List:", str(sys.argv))

if len(sys.argv) < 2:
    print("Error: Need the Json config file as argument")
    sys.exit()

file_name = str(sys.argv[1])
create_gddv()
