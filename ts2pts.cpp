#include "ts2pts.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#if defined(WIN32)
#include <io.h>
#endif

#define MIN(a,b) ((a)<(b)?(a):(b))

#define WRITE_JST // add JST_time @ Partial TS Time Descriptor // tentative

int make_ptsd(unsigned char po[]);
int make_nid(unsigned char po[],
             const unsigned char media_type[],
             const int network_id);
int make_ebd(unsigned char po[],
             const int terrestrial_broadcaster_id);
int make_pttd(unsigned char po[],
              const unsigned char partial_ts_time_d[8]);
int make_sit(PtsInfo_t &pts_info);

int parse_PAT(PtsInfo_t &pts_info,
              const unsigned char pi[],
              const int pgnum);
int parse_PMT(bool pidout[MAX_PID],
              unsigned char pmtdata[TSDATA_SIZE*2],
              const unsigned char pi[]);
int parse_NIT(unsigned char media_type[2],
              unsigned char ts_info_d[MAX_DSC_LEN],
              const unsigned char pi[]);
int parse_TOT(TIME_t &now,
              const unsigned char pi[]);
int parse_EIT(PtsInfo_t &pts_info,
              unsigned char partial_ts_time_d[8],
              const unsigned char pi[],
              const TIME_t now,
              const unsigned int service_id);
int parse_SDT(unsigned char service_d[MAX_DSC_LEN],
              const unsigned char pi[],
              const unsigned int id);
int parse_PSI(PtsInfo_t &pts_info,
              const int mode,
              const unsigned char pi[],
              const int pgnum);

inline unsigned int bswap32(unsigned int x)
{
    x = ((x<<8)&0xff00ff00) | ((x>>8)&0x00ff00ff);
    x = (x>>16) | (x<<16);
    return x;
}

// make Partial Transport Stream Descriptor
int make_ptsd(unsigned char po[])
{
    // set value
    int descriptor_tag = 0x63;
    int descriptor_length = 0x08;
    int peak_rate = 0xea60; // tentative
    int minimum_overall_smoothing_rate   = 0x3FFFF;
    int maximum_overall_smoothing_buffer = 0x3FFF;

    // write
    PUT_BYTE(po,   descriptor_tag);
    PUT_BYTE(po+1, descriptor_length);
    PUT_BYTE(po+2, (0x3<<6) | ((peak_rate>>16)&0x3f) );
    PUT_WORD(po+3, peak_rate);
    PUT_BYTE(po+5, (0x3<<6) | ((minimum_overall_smoothing_rate>>16)&0x3f) );
    PUT_WORD(po+6, minimum_overall_smoothing_rate);
    PUT_WORD(po+8, (0x3<<14) | maximum_overall_smoothing_buffer );

    return 0;
}

// make Network Identification Descriptor
int make_nid(unsigned char po[],
             const unsigned char media_type[],
             const int network_id)
{
    // set value
    int descriptor_tag = 0xC2;
    int descriptor_length = 0x07;
    char country_code[]="JPN";

    // write
    PUT_BYTE(po,   descriptor_tag);
    PUT_BYTE(po+1, descriptor_length);
    PUT_STR (po+2, country_code, 3);
    PUT_STR (po+5, media_type,   2);
    PUT_WORD(po+7, network_id);

    return 0;
}

// make Extended Broadcaster Descriptor
int make_ebd(unsigned char po[],
             const int terrestrial_broadcaster_id)
{
    // set value
    int descriptor_tag    = 0xCE;
    int descriptor_length = 0x5;
    int broadcaster_type  = 0x1;
    int number_of_affiliation_id_loop = 0x1; // tentative
    int number_of_broadcaster_id_loop = 0x0; // tentative

    // write
    PUT_BYTE(po,   descriptor_tag);
    PUT_BYTE(po+1, descriptor_length);
    PUT_BYTE(po+2, (broadcaster_type<<4) | 0xf);
    PUT_WORD(po+3, terrestrial_broadcaster_id);
    PUT_BYTE(po+5, (number_of_affiliation_id_loop<<4) | number_of_broadcaster_id_loop);
    PUT_BYTE(po+6, terrestrial_broadcaster_id&0x0f);

    return 0;
}

// make Partial TS Time Descriptor
int make_pttd(unsigned char po[],
              const unsigned char partial_ts_time_d[8])
{
    // set value
    int descriptor_tag          = 0xC3;
    int descriptor_length       = 0x0D;
    int event_version_number    = 0x00;
    int offset                  = 0x000000;
    int offset_flag             = 0x0;
    int other_descriptor_status = 0x0;
    int JST_time_flag           = 0x0;
#ifdef WRITE_JST
    descriptor_length+=5;
    JST_time_flag = 1;
    
    // ・開始時刻が番組開始時刻と同じ
    // ・SITが1回/1秒で挿入される
    // と仮定、本当はTOTとPCRから計算すべき
    static TIME_t nt = {(unsigned short)((partial_ts_time_d[0]<<8)|partial_ts_time_d[1]), partial_ts_time_d[2],partial_ts_time_d[3],partial_ts_time_d[4]};
    nt.ss++; 

    if ((nt.ss&0xF)==0xA) nt.ss+=6;
    if (nt.ss==0x60) {
        nt.ss=0;
        nt.mm++;
    }
    if ((nt.mm&0xF)==0xA) nt.mm+=6;
    if (nt.mm==0x60) {
        nt.mm=0;
        nt.hh++;
    }
    if ((nt.hh&0xF)==0xA) nt.hh+=6;
    if (nt.hh==0x24) {
        nt.MJD++;
        nt.hh=0;
    }
#endif

    // write
    PUT_BYTE(po,    descriptor_tag);
    PUT_BYTE(po+ 1, descriptor_length);
    PUT_BYTE(po+ 2, event_version_number);
    PUT_STR (po+ 3, partial_ts_time_d, 8);
    PUT_BYTE(po+11, offset>>16);
    PUT_WORD(po+12, offset);
    PUT_BYTE(po+14, 0xf8 | (offset_flag<<2) | (other_descriptor_status<<1) | JST_time_flag);
#ifdef WRITE_JST
    PUT_WORD(po+15, nt.MJD);
    PUT_BYTE(po+17, nt.hh);
    PUT_BYTE(po+18, nt.mm);
    PUT_BYTE(po+19, nt.ss);
#endif

    return 0;
}

// make SIT
int make_sit(PtsInfo_t &pts_info)
{
    int idx;
    unsigned char *po = pts_info.sitdata;
    memset(po, 0, MAX_PSI_LEN); // clear buffer

    ///// transmission descriptor /////
    unsigned char *pb0 = po+0xB; // mark start address
    unsigned char *pb = pb0;
    // Partial Transport Stream Descriptor
    make_ptsd(pb);
    pb+=pb[1]+2;
    // Network Identification Descriptor
    make_nid(pb, pts_info.media_type, pts_info.transport_stream_id);
    pb+=pb[1]+2;
    // TS Information Descriptor
    if(pts_info.ts_info_d[1]!=0){
        memcpy(pb, pts_info.ts_info_d, pts_info.ts_info_d[1]+2); // copy from NIT
        pb+=pb[1]+2;
    }
    int transmission_info_loop_length = (int)(pb-pb0);

    ///// service /////
    // set vaule
    int service_id  = pts_info.service_id;
    int running_status = 0x0;
    // write
    PUT_WORD(pb, service_id);
    pb+=4; // pb[2]&pb[3] write later
    pb0 = pb; // mark start address

    // Extended Broadcaster Descriptor
    if ((pts_info.transport_stream_id&0xFF00)==0x7F00) { // if terrestrial broadcasting
        make_ebd(pb, pts_info.transport_stream_id);
        pb+=pb[1]+2;
    }
    // Partial TS Time Descriptor
    make_pttd(pb, pts_info.partial_ts_time_d);
    pb+=pb[1]+2;
    // Short Event Descriptor
    if (pts_info.short_event_d[0]==0x4D) {
        memcpy(pb, pts_info.short_event_d, pts_info.short_event_d[1]+2); // copy from EIT
        pb+=pb[1]+2;
    }
    // Component Descriptor
    if (pts_info.component_d[0]==0x50) {
        memcpy(pb, pts_info.component_d, pts_info.component_d[1]+2); // copy from EIT
        pb+=pb[1]+2;
    }
    // Audio Component Descriptor
    if (pts_info.audio_component_d[0]==0xC4) {
        memcpy(pb, pts_info.audio_component_d, pts_info.audio_component_d[1]+2); // copy from EIT
        pb+=pb[1]+2;
    }
    // Data Content Descriptor
    idx=0;
    while (pts_info.data_content_d[idx][0]==0xC7) {
        memcpy(pb, pts_info.data_content_d[idx], pts_info.data_content_d[idx][1]+2); // copy from EIT
        idx++;
        pb+=pb[1]+2;
    }
    // Service Descriptor
    if (pts_info.service_d[0]==0x48) {
        memcpy(pb, pts_info.service_d, pts_info.service_d[1]+2); // copy from SDT
        pb+=pb[1]+2;
    }
    // Extended Event Descriptor
    idx=0;
    while (pts_info.extended_event_d[idx][0]==0x4E) {
        memcpy(pb, pts_info.extended_event_d[idx], pts_info.extended_event_d[idx][1]+2); // copy from EIT
        idx++;
        pb+=pb[1]+2;
    }
    // Content Descriptor
    if (pts_info.content_d[0]==0x54) {
        memcpy(pb, pts_info.content_d, pts_info.content_d[1]+2); // copy from SDT
        pb+=pb[1]+2;
    }
    // Event Group Descriptor
    if (pts_info.event_group_d[0]==0xD6) {
        memcpy(pb, pts_info.event_group_d, pts_info.event_group_d[1]+2); // copy from SDT
        pb+=pb[1]+2;
    }
    int service_loop_length = (int)(pb-pb0);
    PUT_WORD(pb0-2, 0x8000 | (running_status<<12) | service_loop_length);

    ///// set & write header /////
    PsiHdr_t hd; hd.dword0=~0;
    hd.table_id                   = 0x7f; // Selection Information Table
    //hd.section_syntax_indicator = 0x1;
    //hd.private_indicator        = 0x1; //reserved_future_use
    //hd.reserved                 = 0x3;
    hd.private_section_length     = (pb-(po+4))+4;

    PsiExHdr_t ehd; ehd.dword0=~0;
    //ehd.table_id_extension      = 0xffff; // reserved_future_use
    //ehd.reserved                = 0x3;
    ehd.version_number            = 0x1;
    //ehd.current_next_indicator  = 0x1;
    ehd.section_number            = 0x0;
    ehd.last_section_number       = 0x0;

    PUT_BYTE (po,   0x00); // pointer_field
    PUT_DWORD(po+1, hd.dword0);
    PUT_DWORD(po+4, ehd.dword0);
    PUT_BYTE (po+8, ehd.last_section_number);
    PUT_WORD (po+9, 0xf000 | transmission_info_loop_length);

    // write crc
    unsigned int crc32 = bswap32(get_crc(po+1, 3+hd.private_section_length-4));
    PUT_DWORD(pb, crc32);

    return 0;
}

// write Partial TS
int write_pts(const char finame[],
              const char foname[],
              PtsInfo_t &pts_info,
                PtsInfo_t& pts_info_meta)
{
    FILE *fi=fopen((const char*)finame, "rb");
    FILE *fo=fopen((const char*)foname, "wb");
    if(fo==NULL){
        printf("write file open error\n");
        return -1;
    }
    printf("converting TS: %s\n"
           "to Partial TS: %s\n", finame, foname);
    unsigned int total = (unsigned int)(_filelengthi64(_fileno(fi))/TSPKT_SIZE);

    unsigned char tsbuf[TSPKT_SIZE];
    int pmt_rlen, sit_rlen;
    int pat_cnt, pmt_cnt, sit_cnt, cnt;
    unsigned char *ppmt, *psit;    
    TsHdr_t pmt_hd, sit_hd;

    // set TS header for PMT & PAT
    pmt_hd.sync_byte                    = 0x47;
    pmt_hd.transport_error_indicator    = 0x0;
    pmt_hd.payload_unit_start_indicator = 0x1;
    pmt_hd.transport_priority           = 0x0;
    pmt_hd.PID                          = pts_info.pmt_pid;
    pmt_hd.transport_scrambling_control = 0x0;
    pmt_hd.adaptation_field_control     = 0x1;
    pmt_hd.continuity_counter           = 0x0;
    sit_hd.sync_byte                    = 0x47;
    sit_hd.transport_error_indicator    = 0x0;
    sit_hd.payload_unit_start_indicator = 0x1;
    sit_hd.transport_priority           = 0x0;
    sit_hd.PID                          = 0x001F;
    sit_hd.transport_scrambling_control = 0x0;
    sit_hd.adaptation_field_control     = 0x1;
    sit_hd.continuity_counter           = 0x0;

    pmt_rlen = sit_rlen = 0;
    pat_cnt = cnt = 0;
    make_sit(pts_info_meta);

    while (fread(tsbuf, 1, TSPKT_SIZE, fi)==TSPKT_SIZE) {
        TsHdr_t tsh = {bswap32(*(unsigned int*)tsbuf)};
        if (tsh.PID==0x0000) { // PAT
            PUT_STR(tsbuf+4, pts_info.patdata, TSDATA_SIZE); // overwrite PAT
            fwrite(tsbuf, 1, TSPKT_SIZE, fo);
            pat_cnt++;
            if (pat_cnt%10==9) { // insert SIT
                // PATが0.1秒毎に存在していると仮定して1秒毎にSITを挿入
                // 本当は、PCRを見た方が良いはず
                // JST_timeを出力しないならば適当で良いかも
#ifdef WRITE_JST
                make_sit(pts_info_meta); // 全部作り直すのは無駄、JST_timeとCRCのみ更新すれば良い
#endif
                PsiHdr_t hd = {bswap32(*(unsigned int*)(pts_info_meta.sitdata+1))};
                sit_rlen = 1+hd.private_section_length+3;
                psit = pts_info_meta.sitdata;
                sit_cnt = 0;

                sit_hd.payload_unit_start_indicator = 1;
                sit_hd.continuity_counter = (sit_hd.continuity_counter+1)%16;
                PUT_DWORD(tsbuf, sit_hd.dword0);
                memset (tsbuf+4, 0xff, TSDATA_SIZE);
                PUT_STR(tsbuf+4, psit, MIN(sit_rlen,TSDATA_SIZE));
                fwrite(tsbuf, 1, TSPKT_SIZE, fo);
                psit+=TSDATA_SIZE;
                sit_rlen-=TSDATA_SIZE;
            }
        }
        else if (tsh.PID==pts_info.pmt_pid) { // PMT
            if (tsh.payload_unit_start_indicator) {
                PsiHdr_t hd = {bswap32(*(unsigned int*)(pts_info.pmtdata+1))};
                pmt_rlen = 1+hd.private_section_length+3;
                ppmt = pts_info.pmtdata;
                pmt_cnt = 0;

                pmt_hd.payload_unit_start_indicator = 1;
                pmt_hd.continuity_counter = (pmt_hd.continuity_counter+1)%16;
                PUT_DWORD(tsbuf, pmt_hd.dword0);
                memset (tsbuf+4, 0xff, TSDATA_SIZE);
                PUT_STR(tsbuf+4, ppmt, MIN(pmt_rlen,TSDATA_SIZE));
                fwrite(tsbuf, 1, TSPKT_SIZE, fo);
                ppmt+=TSDATA_SIZE;
                pmt_rlen-=TSDATA_SIZE;
            }
        }
        else if (pts_info.pidout[tsh.PID]) { // Video, Audio, ...
            fwrite(tsbuf, 1, TSPKT_SIZE, fo);
        }

        // write remaining PMT data
        if (pmt_rlen>0) {
            pmt_cnt++;
            if ((pmt_cnt&0xff)==0) {
                pmt_hd.payload_unit_start_indicator = 0;
                pmt_hd.continuity_counter = (pmt_hd.continuity_counter+1)%16;
                PUT_DWORD(tsbuf, pmt_hd.dword0);
                memset (tsbuf+4, 0xff, TSDATA_SIZE);
                PUT_STR(tsbuf+4, ppmt, MIN(pmt_rlen,TSDATA_SIZE));
                fwrite(tsbuf, 1, TSPKT_SIZE, fo);
                ppmt+=TSDATA_SIZE;
                pmt_rlen-=TSDATA_SIZE;
            }
        }
        // write remaining SIT data
        if (sit_rlen>0) {
            sit_cnt++;
            if ((sit_cnt&0xff)==0) {
                sit_hd.payload_unit_start_indicator = 0;
                sit_hd.continuity_counter = (sit_hd.continuity_counter+1)%16;
                PUT_DWORD(tsbuf, sit_hd.dword0);
                memset (tsbuf+4, 0xff, TSDATA_SIZE);
                PUT_STR(tsbuf+4, psit, MIN(sit_rlen,TSDATA_SIZE));
                fwrite(tsbuf, 1, TSPKT_SIZE, fo);
                psit+=TSDATA_SIZE;
                sit_rlen-=TSDATA_SIZE;
            }
        }
        cnt++;
        if((cnt&0xffff)==0)
            printf("%9d/%9d packets done\r", cnt, total);
    }
    printf("%9d/%9d packets done\n", cnt, total);
    fclose(fi);
    fclose(fo);
    return 0;
}

// parse PAT & modify PAT
int parse_PAT(PtsInfo_t &pts_info,
              const unsigned char pi[],
              const int pgnum)
{
    typedef union {
        unsigned long dword0;
        struct {
            unsigned long pid            : 13;
            unsigned long reserved       :  3;
            unsigned long program_number : 16;
        };
    } PgLst_t;

    // clear write buf
    memset(pts_info.patdata, 0xff, TSDATA_SIZE);
    PUT_BYTE(pts_info.patdata, 0x00); // pointer_field
    unsigned char *ps = pts_info.patdata+1;
    unsigned char *po = ps;

    // parse header
    PsiHdr_t    hd = {bswap32(*(unsigned int*)pi)};
    pi+=3;
    PsiExHdr_t ehd = {bswap32(*(unsigned int*)pi), pi[4]};
    pi+=5;
    int num_pg = (hd.private_section_length-5-4)/4;

    // rewirte section_length
    hd.private_section_length = 17; // only 1 program

    // write header
    pts_info.transport_stream_id = ehd.table_id_extension;
    PUT_DWORD(po,   hd.dword0);
    PUT_DWORD(po+3, ehd.dword0);
    PUT_BYTE (po+7, ehd.last_section_number);
    po+=8;
    // write SIT info.
    PgLst_t sit_pg={0xFFFFFFFF};
    sit_pg.pid            = 0x001f;
    sit_pg.program_number = 0x0000;
    PUT_DWORD(po, sit_pg.dword0);
    po+=4;

    // program loop
    int pg_cnt=0;
    for (int i=0; i<num_pg; i++) {
        // parse
        PgLst_t pg={bswap32(*(unsigned int*)pi)};
        if (pg.program_number!=0) {
            if (pg_cnt==pgnum) {
                pts_info.pidout[pg.pid]=true;
                pts_info.pmt_pid    = pg.pid;
                pts_info.service_id = pg.program_number;
                // write PMT info.
                PUT_DWORD(po, pg.dword0);
                po+=4;
                break;
            }
            else {
                pg_cnt++;
            }
        }
        pi+=4;
    }
    // set crc
    unsigned int crc32 = bswap32(get_crc(ps, 3+hd.private_section_length-4));
    PUT_DWORD(po, crc32);

    return 0;
}

// parse PMT & modify PMT
int parse_PMT(bool pidout[MAX_PID],
              unsigned char pmtdata[TSDATA_SIZE*2],
              const unsigned char pi[])
{
const unsigned char dtcp_descriptor[4][6] = {
    {0x88,0x04,0x0F,0xFF,0xFC,0xFC}, // Copy-free
    {0x88,0x04,0x0F,0xFF,0xFD,0xFC}, // No-more-copies
    {0x88,0x04,0x0F,0xFF,0xFE,0xFC}, // Copy-one-generation
    {0x88,0x04,0x0F,0xFF,0xFF,0xFC}  // Copy-Never
};

#define ADD_DTCP(p,i) \
    do{\
        memcpy(p, dtcp_descriptor[i], sizeof(dtcp_descriptor[0]));\
        p+= sizeof(dtcp_descriptor[0]);\
    }while(0)

#define COPY_DSC(po,pi) \
    do{\
      memcpy(po, pi, pi[1]+2);\
      po+=pi[1]+2;\
    }while(0)

    typedef union {
        unsigned long dword0;
        struct {
            unsigned long len       : 12;
            unsigned long reserved1 :  4;
            unsigned long pid       : 13;
            unsigned long reserved0 :  3;
        };
    } Pmt_t;

    // init write buf
    memset(pmtdata, 0xff, TSDATA_SIZE*2);
    PUT_BYTE(pmtdata, 0x00); // pointer_field
    unsigned char *ps  = pmtdata+1;
    unsigned char *pss, *po;
    int w_sec_len, w_dsc_len;

    // parse header
    PsiHdr_t    hd = {bswap32(*(unsigned int*)pi)};
    pi+=3;
    PsiExHdr_t ehd = {bswap32(*(unsigned int*)pi), pi[4]};
    pi+=5;
    Pmt_t    data0 = {bswap32(*(unsigned int*)pi)};
    pi+=4;

    pidout[data0.pid]=true;
    w_sec_len = hd.private_section_length;
    w_dsc_len = data0.len;
    pss = ps+8;
    po  = pss+4;

    // transmission descriptor
    ADD_DTCP(po,2); // add DTCP descriptor (force CopyOnce)
    w_sec_len += sizeof(dtcp_descriptor[0]);
    w_dsc_len += sizeof(dtcp_descriptor[0]);

    int rlen = data0.len;
    while (rlen>0) {
        if (pi[0]!=0x09) {
            COPY_DSC(po,pi);
        }
        else {
            // remove Conditional Access Descriptor
            w_sec_len -= pi[1]+2;
            w_dsc_len -= pi[1]+2;
        }
        rlen -= pi[1]+2;
        pi   += pi[1]+2;
    }

    rlen = hd.private_section_length-5-4-data0.len-4;

    data0.len = w_dsc_len;
    PUT_DWORD(pss, data0.dword0);

    while (rlen>0) {
        // parse
        unsigned char stream_type = pi[0];
        Pmt_t data1 = {bswap32(*(unsigned int*)(pi+1))};
        rlen-=5;
        pi+=5;

        pidout[data1.pid]=true;
        w_dsc_len = data1.len;
        pss = po;
        po += 5;

        // elementary descriptor
        ADD_DTCP(po,2); // add DTCP descriptor (force CopyOnce)
        w_sec_len += sizeof(dtcp_descriptor[0]);
        w_dsc_len += sizeof(dtcp_descriptor[0]);

        int rlen1 = data1.len;
        while (rlen1>0) {
            if (pi[0]!=0x09) {
                COPY_DSC(po,pi);
            }
            else {
                // remove Conditional Access Descriptor
                w_sec_len -= pi[1]+2;
                w_dsc_len -= pi[1]+2;
            }
            rlen  -= pi[1]+2;
            rlen1 -= pi[1]+2;
            pi    += pi[1]+2;
        }

        data1.len = w_dsc_len;
        PUT_BYTE (pss,   stream_type);
        PUT_DWORD(pss+1, data1.dword0);
    }

    // write header
    hd.private_section_length = w_sec_len;
    PUT_DWORD(ps,   hd.dword0);
    PUT_DWORD(ps+3, ehd.dword0);
    PUT_BYTE (ps+7, ehd.last_section_number);
    // set crc
    unsigned int crc32 = bswap32(get_crc(ps, 3+hd.private_section_length-4));
    PUT_DWORD(po, crc32);

    return 0;
}

// parse NIT
int parse_NIT(unsigned char media_type[2],
              unsigned char ts_info_d[MAX_DSC_LEN],
              const unsigned char pi[])
{
    // parse header
    PsiHdr_t    hd = {bswap32(*(unsigned int*)pi)};
    pi+=3;
    PsiExHdr_t ehd = {bswap32(*(unsigned int*)pi), pi[4]};
    pi+=5;

    int network_descriptor_length = ((pi[0]&0xf)<<8) | pi[1];
    pi+=2;
    int rlen = network_descriptor_length;
    while (rlen>0) {
        if (pi[0]==0xFE) { // System Management Descriptor
            int broadcasting_identifier = pi[2]&0x3f;
            switch(broadcasting_identifier){ // tentative
            case 1:
            case 7:
                memcpy(media_type, "CS", 2);
                break;
            case 2:
            case 4:
                memcpy(media_type, "BS", 2);
                break;
            case 3:
            case 5:
                memcpy(media_type, "TB", 2);
                break;
            default:
                printf("unknown broadcasting_identifier=%#x\n", broadcasting_identifier);
                return -1;
            }
        }
        rlen  -= pi[1]+2;
        pi    += pi[1]+2;
    }

    int transport_stream_descriptor_length = ((pi[0]&0xf)<<8) | pi[1];
    pi+=2;
    rlen = transport_stream_descriptor_length;
    while (rlen>0) {
        int transport_descriptor_length = ((pi[4]&0xf)<<8) | pi[5];
        pi+=6;
        int rlen2=transport_descriptor_length;
        while (rlen2>0) {
            if (pi[0]==0xCD) { // TS Information Descriptor
                memcpy(ts_info_d, pi, pi[1]+2);
            }
            rlen2 -= pi[1]+2;
            pi    += pi[1]+2;
        }
        rlen -= transport_descriptor_length+6;
    }
    return 0;
}

// parse TOT
int parse_TOT(TIME_t &now,
              const unsigned char pi[])
{
    // parse header
    PsiHdr_t hd = {bswap32(*(unsigned int*)pi)};
    pi+=3;

    if (hd.table_id!=0x73) {
        printf("not TOT");
        return -1;
    }

    now.MJD = (pi[0]<<8) | pi[1];
    now.hh  = pi[2];
    now.mm  = pi[3];
    now.ss  = pi[4];

    return 0;
}

// parse EIT
int parse_EIT(PtsInfo_t &pts_info,
              unsigned char partial_ts_time_d[8],
              const unsigned char pi[],
              const TIME_t now,
              const unsigned int service_id)
{
    // parse header
    PsiHdr_t hd = {bswap32(*(unsigned int*)pi)};
    pi+=3;
    PsiExHdr_t ehd = {bswap32(*(unsigned int*)pi), pi[4]};
    pi+=5;

    if (hd.table_id!=0x4E ||
        ehd.table_id_extension!=service_id ||
        hd.private_section_length==0x0f) {
        return 1;
    }
    pi+=6;

    int cur_eit = 0;
    int dc_idx  = 0;
    int ee_idx  = 0;
    int rlen = hd.private_section_length - (11+4);
    while (rlen>0) {
        int event_id   = (pi[0x0]<<8) | pi[0x1];
        TIME_t start_time;
        start_time.MJD              = (pi[0x2]<<8) | pi[0x3];
        start_time.hh               = pi[0x4];
        start_time.mm               = pi[0x5];
        start_time.ss               = pi[0x6];

        TIME_t duration;
        duration.hh                 = pi[0x7];
        duration.mm                 = pi[0x8];
        duration.ss                 = pi[0x9];

        int st = 60*60*start_time.hh + 60*start_time.mm + start_time.ss;
        int et = 60*60*duration.hh   + 60*duration.mm   + duration.ss + st;
        int nt = 24*60*60*(now.MJD-start_time.MJD)
               + 60*60*now.hh + 60*now.mm + now.ss;
        if (st<=nt && nt<=et) {
            memcpy(partial_ts_time_d, pi+2, 8);
            cur_eit=1;
        }

        int running_status          = (pi[0x0a]>>5)&0x7;
        int free_CA_mode            = (pi[0x0a]>>4)&0x1;
        int descriptors_loop_length = ((pi[0x0a]&0xf)<<8) | pi[0x0b];

        pi+=0x0c;

        int rlen2 = descriptors_loop_length;
        while (rlen2>0) {
            int descriptor_length = pi[1]+2;
            if (cur_eit) {
                int descriptor_tag = pi[0];
                switch (descriptor_tag) {
                case 0x4D: // Short Event Descriptor
                    memcpy(pts_info.short_event_d, pi, descriptor_length);
                    break;
                case 0x50: // Component Descriptor
                    memcpy(pts_info.component_d, pi, descriptor_length);
                    break;
                case 0xC4: // Audio Component Descriptor
                    memcpy(pts_info.audio_component_d, pi, descriptor_length);
                    break;
                case 0xC7: // Data Content Descriptor
                    memcpy(pts_info.data_content_d[dc_idx], pi, descriptor_length);
                    dc_idx++;
                    break;
                case 0x4E: // Extended Event Descriptor
                    memcpy(pts_info.extended_event_d[ee_idx], pi, descriptor_length);
                    ee_idx++;
                    break;
                case 0x54: // Content Descriptor
                    memcpy(pts_info.content_d, pi, descriptor_length);
                    break;
                case 0xD6: // Event Group Descriptor
                    memcpy(pts_info.event_group_d, pi, descriptor_length);
                    break;
                }
            }
            rlen2 -= descriptor_length;
            pi    += descriptor_length;
        }
        rlen -= descriptors_loop_length+0x0c;

        if (cur_eit) {
            return 0;
        }
    }

    return 1;
}

// parse SDT
int parse_SDT(unsigned char service_d[MAX_DSC_LEN],
              const unsigned char pi[],
              const unsigned int id)
{
    // parse header
    PsiHdr_t hd = {bswap32(*(unsigned int*)pi)};
    pi+=3;
    PsiExHdr_t ehd = {bswap32(*(unsigned int*)pi), pi[4]};
    pi+=5;
    pi+=3;
    int rlen = hd.private_section_length-(8+4);

    while (rlen>0) {
        int service_id              = (pi[0]<<8) | pi[1];
        int descriptors_loop_length = ((pi[3]&0xf)<<8) | pi[4];
        pi+=5;

        // service descriptor
        int rlen2=descriptors_loop_length;
        while (rlen2>0) {
            if (service_id==id && pi[0]==0x48) { // Service Descriptor
                memcpy(service_d, pi, pi[1]+2);
                return 0;
            }
            rlen2 -= pi[1]+2;
            pi    += pi[1]+2;
        }
        rlen -= descriptors_loop_length+5;
    }

    return 1;
}

// parse PSI
int parse_PSI(PtsInfo_t &pts_info,
              const int mode,
              const unsigned char pi[],
              const int pgnum)
{
    int ret;
    switch (mode) {
    case 0: // PAT
        ret = parse_PAT(pts_info, pi, pgnum);
        if(ret==0) printf("PAT:OK\n");
        break;
    case 1: // PMT
        ret = parse_PMT(pts_info.pidout, pts_info.pmtdata, pi);
        if(ret==0) printf("PMT:OK\n");
        break;
    case 2: // NIT
        ret = parse_NIT(pts_info.media_type, pts_info.ts_info_d, pi);
        if(ret==0) printf("NIT:OK\n");
        break;
    case 3: // TOT
        ret = parse_TOT(pts_info.now, pi);
        if(ret==0) printf("TOT:OK\n");
        break;
    case 4: // EIT
        ret = parse_EIT(pts_info, pts_info.partial_ts_time_d, pi, pts_info.now, pts_info.service_id);
        if(ret==0) printf("EIT:OK\n");
        break;
    case 5: // SDT
        ret = parse_SDT(pts_info.service_d, pi, pts_info.service_id);
        if(ret==0) printf("SDT:OK\n");
        break;
    }
    return ret;
}

// analyze TS
int analyze_ts(PtsInfo_t &pts_info,
               const char finame[],
               const int skipnum,
               const int pgnum)
{
    typedef struct PSI_BUF_s {
        int  rem_bytes;
        unsigned char *pcur;
        unsigned char *buf;
    } PSI_BUF_t;

    FILE *fi=fopen(finame, "rb");
    if (fi==NULL) {
        printf("input file open error: %s\n", finame);
        return -1;
    }
    if (skipnum==-1) { // auto seting (start from center)
        __int64 fl = _filelengthi64(_fileno(fi));
        fl = (fl/(2*TSPKT_SIZE)*TSPKT_SIZE);
#if defined(WIN32)
        fsetpos(fi, &fl);
#else
        fsetpos(fi, (fpos_t *)&fl);
#endif
    }
    else{ // manual setting
        fseek(fi, skipnum*TSPKT_SIZE, SEEK_SET);
    }

    printf("analyzing TS: %s\n", finame);

    // init.
    unsigned char tsbuf[TSPKT_SIZE];
    PSI_BUF_t psi;
    psi.buf=NULL;
    int prev_cnt=0xff;
    int ret, mode=0;
    memset(&pts_info, 0, sizeof(pts_info));
    init_crc(); // initialize CRC table

    while (fread(tsbuf, 1, TSPKT_SIZE, fi)==TSPKT_SIZE) {
        TsHdr_t tsh = {bswap32(*(unsigned int*)tsbuf)};

        // !!!not check error!!!

        if (mode==0 && tsh.PID==0x0000 || // PAT
            mode==1 && tsh.PID==pts_info.pmt_pid || // PMT
            mode==2 && tsh.PID==0x0010 || // NIT
            mode==3 && tsh.PID==0x0014 || // TOT
#if 1
            mode==4 && tsh.PID==0x0012 || // EIT
#else
            mode==4 && tsh.PID==0x0027 || // EIT
#endif
            mode==5 && tsh.PID==0x0011    // SDT
           ) {
            unsigned char *pi = tsbuf+4;
            int adp=0;
            if (tsh.adaptation_field_control&2) {
                adp = pi[1]+1;
            }
            pi+=adp;
            int rsize = TSDATA_SIZE-adp;

            if (tsh.payload_unit_start_indicator) {
                int pointer_field = pi[0];
                if (pointer_field && psi.buf!=NULL) {
                    memcpy(psi.pcur, pi+1, psi.rem_bytes);
                    // buffer filled
                    ret = parse_PSI(pts_info, mode, psi.buf, pgnum);
                    delete[] psi.buf; psi.buf=NULL;
                    if (ret==-1) {
                        goto END_ANALYZE;
                    }
                    if (mode<4 || ret==0)
                        mode++;
                    if (mode==6) {
                        goto END_ANALYZE;
                    }
                }
                else {
                    pi    += pointer_field+1;
                    rsize -= pointer_field+1;

                    PsiHdr_t hd = {bswap32(*(unsigned int*)pi)};
                    int ssize = 3+hd.private_section_length;
                    psi.buf       = new unsigned char[ssize];
                    psi.pcur      = psi.buf;
                    psi.rem_bytes = ssize;
                }
            }

            // accumulate
            if (psi.rem_bytes && psi.buf!=NULL) {
                rsize = MIN(psi.rem_bytes, rsize);
                memcpy(psi.pcur, pi, rsize);
                psi.rem_bytes -= rsize;
                psi.pcur      += rsize;

                if(psi.rem_bytes==0){ // buffer filled
                    ret = parse_PSI(pts_info, mode, psi.buf, pgnum);
                    delete[] psi.buf; psi.buf=NULL;
                    if (ret==-1) {
                        goto END_ANALYZE;
                    }
                    if (mode<4 || ret==0)
                        mode++;
                    if (mode==6) {
                        goto END_ANALYZE;
                    }
                }
            }
        }
    }

    printf("not exist PAT/PMT/NIT/TOT/EIT/SDT\n");
END_ANALYZE:
    fclose(fi);
    return mode>4 ? 0 : -1;
}
