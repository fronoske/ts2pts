#ifndef MPEG2TS_H
#define MPEG2TS_H

#define TSPKT_SIZE      188UL       // TSパケットのバイト数
#define TSDATA_SIZE     184UL       // TSパケットのヘッダを除いたバイト数
#define MAX_PID         0x2000UL    // PID最大値
#define MAX_DSC_LEN     0x100UL     // Descriptorの最大長
#define MAX_PSI_LEN     0x1000UL    // PSIの最大長

// TSヘッダ 4BYTE
typedef union TS_HDR_u {
    unsigned long dword0;
    struct {
        unsigned long continuity_counter           :  4;
        unsigned long adaptation_field_control     :  2;
        unsigned long transport_scrambling_control :  2;
        unsigned long PID                          : 13;
        unsigned long transport_priority           :  1;
        unsigned long payload_unit_start_indicator :  1;
        unsigned long transport_error_indicator    :  1;
        unsigned long sync_byte                    :  8;
    };
} TsHdr_t;

// PSIヘッダ 3BYTE + ダミー 1BYTE
typedef union PSI_HDR_u {
    unsigned long dword0;
    struct {
        unsigned long dummy                    :  8;
        unsigned long private_section_length   : 12;
        unsigned long reserved                 :  2;
        unsigned long private_indicator        :  1;
        unsigned long section_syntax_indicator :  1;
        unsigned long table_id                 :  8;
    };
} PsiHdr_t;

// PSI拡張ヘッダ 5BYTE
typedef struct PSI_EX_HDR_s {
    union {
        unsigned long dword0;
        struct {
            unsigned long section_number         :  8;
            unsigned long current_next_indicator :  1;
            unsigned long version_number         :  5;
            unsigned long reserved               :  2;
            unsigned long table_id_extension     : 16;
        };
    };
    unsigned char last_section_number;
} PsiExHdr_t;

#endif // MPEG2TS_H
