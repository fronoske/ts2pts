#ifndef TS2PTS_H
#define TS2PTS_H

#include "crc.h"
#include "mpeg2ts.h"
#include "bufwrite.h"

#include "portable.h"

typedef struct TIME_s {
    unsigned short MJD; // Modified Julian Date
    unsigned char hh;   // hour
    unsigned char mm;   // minute
    unsigned char ss;   // second
} TIME_t;

typedef struct PTS_INFO_s {
    ///// TS info /////
    bool pidout[MAX_PID]; // if output 1, else 0
    unsigned int pmt_pid; // PMT's PID (get from PAT)

    ///// modified TS data /////
    // make from PAT
    unsigned char patdata[TSDATA_SIZE];
    // make from PMT
    unsigned char pmtdata[TSDATA_SIZE*2];

    ///// TS info. for SIT /////
    // get from PAT
    unsigned int transport_stream_id;
    unsigned int service_id;
    // make from NIT
    unsigned char media_type[2];
    // get from TOT
    TIME_t now;

    ///// descriptor for SIT /////
    // get from NIT
    unsigned char ts_info_d[MAX_DSC_LEN];
    // get from EIT
    unsigned char partial_ts_time_d[8];
    // get from SDT
    unsigned char service_d[MAX_DSC_LEN];
    // get from EIT
    unsigned char short_event_d[MAX_DSC_LEN];
    unsigned char component_d[MAX_DSC_LEN];
    unsigned char audio_component_d[MAX_DSC_LEN];
    unsigned char data_content_d[16/*tentative*/][MAX_DSC_LEN];
    unsigned char extended_event_d[16][MAX_DSC_LEN];
    unsigned char content_d[MAX_DSC_LEN];
    unsigned char event_group_d[MAX_DSC_LEN];

    // output(SIT) buffer
    unsigned char sitdata[MAX_PSI_LEN]; 
} PtsInfo_t;

int analyze_ts(PtsInfo_t &pts_info,
               const char finame[],
               const int skipnum=-1,
               const int pgnum=0);

int write_pts(const char finame[],
              const char foname[],
              PtsInfo_t &pts_info);

#endif // TS2PTS_H
