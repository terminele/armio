#!/bin/python
import argparse

CHARS_PER_UINT32=5
MAX_CHARS=45
INTS = int(MAX_CHARS/CHARS_PER_UINT32)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('plaintext')
    parser.add_argument('-k', '--key', default=None)
    parser.add_argument('-r', '--rot13', action='store_true', default=False)
    parser.add_argument('-i', '--index', type=int, default=0)
    args = parser.parse_args()
    text = args.plaintext
    
    if len(text) > MAX_CHARS:
        print("Too long. {} chars > max of {}".format(len(text), MAX_CHARS))
        exit()

    ints = [0 for i in range(INTS) ]
    cipher = ""
    for i, c in enumerate(text):
        if c == ' ':
            v = 0
        elif ord(c) >= ord('A') and ord(c) <= ord('Z'):
            v = ord(c) - ord('A') + 1
        elif ord(c) >= ord('a') and ord(c) <= ord('z'):
            v = ord(c) - ord('a') + 1
        else:
            print("Invalid character '{}'".format(c))
            exit()
        
        if args.rot13: 
            if v >=1 and v <= 13:
                v+=13
            elif v > 13 and v <= 26:
                v-=13
            cipher+=chr(ord('A') + v - 1)
        elif args.key:
            o = ord(args.key[i % len(args.key)])
            if o >= ord('A') and o <= ord('Z'):
                key_offset = o - ord('A') 
            elif o >= ord('a') and o <= ord('z'):
                key_offset = o - ord('a')
            elif o >= ord('0') and o <= ord('9'):
                key_offset = o - ord('0')
            else:
                print("Invalid key character '{}'".format(args.key[i]))
                exit()
            if v != 0:
                v+=key_offset
                v = v % 26
                if v == 0:
                    v = 26
            cipher+=chr(ord('A') + v - 1)
        
        ints[int(i/CHARS_PER_UINT32)]+=v*pow(100, i % CHARS_PER_UINT32)
    
    if len(cipher):
        print("//cipher:{}".format(cipher))

    print("            case {}:".format(args.index))
    for i in range(INTS):
        print("                disp_vals[{}] = {}UL;".format(i, ints[i]))

    print("                ee_submode_tic = char_disp_mode_tic;")
    print("                break;")
