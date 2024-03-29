﻿#include <stdio.h>
#include <stdlib.h>

#if defined(WIN32)
#include "getopt.h"
#else
#include <getopt.h>
#endif

#include "ts2pts.h"

int main(int argc, char* argv[])
{
    // set opt
    char const* finame="in.ts";
    char const* fmname="";
    char const* foname="out.ts";
    int offset=-1;
    int pgnum=0;

    ///// TS2PTS /////
    int ret;
    PtsInfo_t pts_info, pts_info_meta;

    int c;
    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"input", 1, 0, 'i'},
            {"meta", 1, 0, 'm'},
            {"output", 1, 0, 'o'},
            {"program", 1, 0, 'p'},
            {"offset", 1, 0, 's'},
            {"help", 0, 0, 'h'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "i:m:o:p:s:h", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            fprintf(stderr, "(%x) option %s", c, long_options[option_index].name);
            if (optarg)
                fprintf(stderr, " with arg %s", optarg);
            fprintf(stderr, "\n");
            break;

        case 'i':
            finame = strdup(optarg);
            break;

        case 'm':
            fmname = strdup(optarg);
            break;

        case 'o':
            foname = strdup(optarg);
            break;

        case 'p':
            pgnum = strtol(optarg, NULL, 0);
            fprintf(stderr, "program: %d\n", pgnum);
            break;

        case 's':
            offset = strtol(optarg, NULL, 0);
            fprintf(stderr, "offset: %d\n", offset);
            break;

        case 'h':
            fprintf(stderr, "\n"
                    "USAGE: ts2pts [-i in.ts] [-o out.ts] [-p Nth_program] [-s offset]\n\n"
                    "  -i, --input in.ts         : input TS file (default: in.ts)\n"
                    "  -m, --meta meta.ts        : meta TS file (optional)\n"
                    "  -o, --output out.ts       : output PTS file (default out.ts)\n"
                    "  -p, --program Nth_program : Nth program in TS stream\n"
                    "  -s, --offset offset       : number of TS packets where to start analysis (default: mid of the input file)\n"
                    "  -h, --help                : show this help\n"
                    "\n");
            exit(1);

        case '?':
            break;

        default:
            fprintf(stderr, "?? getopt returned character code 0%o ??\n", c);
        }
    }

    // analyze in.ts
    ret = analyze_ts(pts_info, finame, offset, pgnum);
    // analyze meta.ts if specified
    if (fmname[0] != '\0' && strcmp(finame, fmname) != 0) {
        ret = analyze_ts(pts_info_meta, fmname, offset, pgnum);
    }
    else {
        pts_info_meta = pts_info;
    }
    if(ret) return -1;
    // write Partial TS
    write_pts(finame, foname, pts_info, pts_info_meta);

    return 0;
}
