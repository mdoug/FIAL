import sys
import json

def generate_segment (out, items, prefix, counter):
    i = counter
    for item in items:
        out.write (prefix + item + "\t\t" + str(i) + "\n")
        i = i + 1
    out.write("\n")
    return i

def print_usage ():
        print("""Usage: python gen_defines.py in_file outfile prefix-type
Prefix-type can be "full", "short" or "none"
""")

def main ():

    args = sys.argv

    if len(args) != 4:
        print_usage()
        return 1

    in_file_name  = args[1]
    out_file_name = args[2]
    prefix_type   = args[3]

    f = open(in_file_name, "r")
    d = json.load(f)
    f.close()

    out = open(out_file_name, "w")

    prefix = "#define "

    if prefix_type == "full":
        prefix = prefix + d["prefix"] + "_" + d["type"] + "_"
        define_symbol = d["prefix"] + "_" + d["type"] + "_FULL" + "_H"
    elif prefix_type == "short":
        prefix = prefix + d["type"] + "_"
        define_symbol = d["prefix"] + "_" + d["type"] + "_SHORT" + "_H"
    elif prefix_type == "none":
        define_symbol = d["prefix"] + "_" + d["type"] + "_NONE" + "_H"
        pass
    else :
        print_usage()
        return 1

    out.write("""
/*************************************************************
 *
 *  This file is automatically generated.  Do not edit!
 *
 *  rather, edit "ast_defines.json" and generate using "gen_defines.py"
 *  in the ../utils folder
 *
 *************************************************************/


""")

    out.write("#ifndef " + define_symbol + "\n")
    out.write("#define " + define_symbol + "\n\n\n")

    i = 0

    segments = d["segments"]
    for seg in segments:
        i = generate_segment(out, d[seg], prefix, i)

    out.write("\n#endif /* " + define_symbol + " */\n")
    out.close()


if __name__ == "__main__":
    main()
