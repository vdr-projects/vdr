//////////////////////////////////////////////////////////////
///                                                        ///
/// si_tables.h: definitions for data structures of the    ///
///              incoming SI data stream                   ///
///                                                        ///
//////////////////////////////////////////////////////////////

// $Revision: 1.4 $
// $Date: 2003/02/04 18:45:36 $
// $Author: hakenes $
//
//   (C) 2001-03 Rolf Hakenes <hakenes@hippomi.de>, under the
//               GNU GPL with contribution of Oleg Assovski,
//               www.satmania.com
//
// libsi is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// libsi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You may have received a copy of the GNU General Public License
// along with libsi; see the file COPYING.  If not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.

#define HILO(x) (x##_hi << 8 | x##_lo)

#define MjdToEpochTime(x) (((x##_hi << 8 | x##_lo)-40587)*86400)
#define BcdTimeToSeconds(x) ((3600 * ((10*((x##_h & 0xF0)>>4)) + (x##_h & 0xF))) + \
                             (60 * ((10*((x##_m & 0xF0)>>4)) + (x##_m & 0xF))) + \
                             ((10*((x##_s & 0xF0)>>4)) + (x##_s & 0xF)))
#define BcdTimeToMinutes(x) ((60 * ((10*((x##_h & 0xF0)>>4)) + (x##_h & 0xF))) + \
                             (((10*((x##_m & 0xF0)>>4)) + (x##_m & 0xF))))
#define BcdCharToInt(x) (10*((x & 0xF0)>>4) + (x & 0xF))
#define CheckBcdChar(x) ((((x & 0xF0)>>4) <= 9) && \
                         ((x & 0x0F) <= 9))
#define CheckBcdSignedChar(x) ((((x & 0xF0)>>4) >= 0) && (((x & 0xF0)>>4) <= 9) && \
                         ((x & 0x0F) >= 0) && ((x & 0x0F) <= 9))

#define TableHasMoreSections(x) (((pat_t *)(x))->last_section_number > ((pat_t *)(x))->section_number)
#define GetTableId(x) ((pat_t *)(x))->table_id
#define GetSectionNumber(x) ((pat_t *)(x))->section_number
#define GetLastSectionNumber(x) ((pat_t *)(x))->last_section_number
#define GetServiceId(x) (((eit_t *)(x))->service_id_hi << 8) | ((eit_t *)(x))->service_id_lo
#define GetSegmentLastSectionNumber(x) ((eit_t *)(x))->segment_last_section_number
#define GetLastTableId(x) ((eit_t *)(x))->segment_last_table_id
#define GetSectionLength(x) HILO(((pat_t *)(x))->section_length)

/*
 *
 *    ETSI ISO/IEC 13818-1 specifies SI which is referred to as PSI. The PSI
 *    data provides information to enable automatic configuration of the
 *    receiver to demultiplex and decode the various streams of programs
 *    within the multiplex. The PSI data is structured as four types of table.
 *    The tables are transmitted in sections.
 *
 *    1) Program Association Table (PAT):
 *
 *       - for each service in the multiplex, the PAT indicates the location
 *         (the Packet Identifier (PID) values of the Transport Stream (TS)
 *         packets) of the corresponding Program Map Table (PMT).
 *         It also gives the location of the Network Information Table (NIT).
 *
 */

#define PAT_LEN 8

typedef struct {
   u_char table_id                               :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char dummy                                  :1;        // has to be 0
   u_char                                        :2;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :2;
   u_char dummy                                  :1;        // has to be 0
   u_char section_syntax_indicator               :1;
#endif
   u_char section_length_lo                      :8;
   u_char transport_stream_id_hi                 :8;
   u_char transport_stream_id_lo                 :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :2;
   u_char version_number                         :5;
   u_char current_next_indicator                 :1;
#else
   u_char current_next_indicator                 :1;
   u_char version_number                         :5;
   u_char                                        :2;
#endif
   u_char section_number                         :8;
   u_char last_section_number                    :8;
} pat_t;

#define PAT_PROG_LEN 4

typedef struct {
   u_char program_number_hi                      :8;
   u_char program_number_lo                      :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :3;
   u_char network_pid_hi                         :5;
#else
   u_char network_pid_hi                         :5;
   u_char                                        :3;
#endif
   u_char network_pid_lo                         :8; 
   /* or program_map_pid (if prog_num=0)*/
} pat_prog_t;

/*
 *
 *    2) Conditional Access Table (CAT):
 *
 *       - the CAT provides information on the CA systems used in the
 *         multiplex; the information is private and dependent on the CA
 *         system, but includes the location of the EMM stream, when
 *         applicable.
 *
 */
#define CAT_LEN 8

typedef struct {
   u_char table_id                               :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char dummy                                  :1;        // has to be 0
   u_char                                        :2;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :2;
   u_char dummy                                  :1;        // has to be 0
   u_char section_syntax_indicator               :1;
#endif
   u_char section_length_lo                      :8;
   u_char reserved_1                             :8;
   u_char reserved_2                             :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :2;
   u_char version_number                         :5;
   u_char current_next_indicator                 :1;
#else
   u_char current_next_indicator                 :1;
   u_char version_number                         :5;
   u_char                                        :2;
#endif
   u_char section_number                         :8;
   u_char last_section_number                    :8;
} cat_t;

/*
 *
 *    3) Program Map Table (PMT):
 *
 *       - the PMT identifies and indicates the locations of the streams that
 *         make up each service, and the location of the Program Clock
 *         Reference fields for a service.
 *
 */

#define PMT_LEN 12

typedef struct {
   u_char table_id                               :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char dummy                                  :1; // has to be 0
   u_char                                        :2;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :2;
   u_char dummy                                  :1; // has to be 0
   u_char section_syntax_indicator               :1;
#endif
   u_char section_length_lo                      :8;
   u_char program_number_hi                      :8;
   u_char program_number_lo                      :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :2;
   u_char version_number                         :5;
   u_char current_next_indicator                 :1;
#else
   u_char current_next_indicator                 :1;
   u_char version_number                         :5;
   u_char                                        :2;
#endif
   u_char section_number                         :8;
   u_char last_section_number                    :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :3;
   u_char PCR_PID_hi                             :5;
#else
   u_char PCR_PID_hi                             :5;
   u_char                                        :3;
#endif
   u_char PCR_PID_lo                             :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :4;
   u_char program_info_length_hi                 :4;
#else
   u_char program_info_length_hi                 :4;
   u_char                                        :4;
#endif
   u_char program_info_length_lo                 :8;
   //descriptors
} pmt_t;

#define PMT_INFO_LEN 5

typedef struct {
   u_char stream_type                            :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :3;
   u_char elementary_PID_hi                      :5;
#else
   u_char elementary_PID_hi                      :5;
   u_char                                        :3;
#endif
   u_char elementary_PID_lo                      :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :4;
   u_char ES_info_length_hi                      :4;
#else
   u_char ES_info_length_hi                      :4;
   u_char                                        :4;
#endif
   u_char ES_info_length_lo                      :8;
   // descriptors
} pmt_info_t;

/*
 *
 *    4) Network Information Table (NIT):
 *
 *       - the NIT is intended to provide information about the physical
 *         network. The syntax and semantics of the NIT are defined in
 *         ETSI EN 300 468.
 *
 */

#define NIT_LEN 10 

typedef struct { 
   u_char table_id                               :8; 
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1; 
   u_char                                        :3; 
   u_char section_length_hi                      :4; 
#else 
   u_char section_length_hi                      :4; 
   u_char                                        :3; 
   u_char section_syntax_indicator               :1; 
#endif
   u_char section_length_lo                      :8; 
   u_char network_id_hi                          :8; 
   u_char network_id_lo                          :8; 
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :2; 
   u_char version_number                         :5; 
   u_char current_next_indicator                 :1; 
#else
   u_char current_next_indicator                 :1; 
   u_char version_number                         :5; 
   u_char                                        :2; 
#endif
   u_char section_number                         :8; 
   u_char last_section_number                    :8; 
#if BYTE_ORDER == BIG_ENDIAN 
   u_char                                        :4; 
   u_char network_descriptor_length_hi           :4; 
#else
   u_char network_descriptor_length_hi           :4; 
   u_char                                        :4; 
#endif
   u_char network_descriptor_length_lo           :8; 
  /* descriptors */
} nit_t; 
 
#define SIZE_NIT_MID 2 

typedef struct {                                 // after descriptors 
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :4; 
   u_char transport_stream_loop_length_hi        :4; 
#else
   u_char transport_stream_loop_length_hi        :4; 
   u_char                                        :4; 
#endif
   u_char transport_stream_loop_length_lo        :8; 
} nit_mid_t; 
 
#define SIZE_NIT_END 4 

struct nit_end_struct { 
   long CRC; 
}; 
 
#define NIT_TS_LEN 6 

typedef struct { 
   u_char transport_stream_id_hi                 :8; 
   u_char transport_stream_id_lo                 :8; 
   u_char original_network_id_hi                 :8; 
   u_char original_network_id_lo                 :8; 
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :4; 
   u_char transport_descriptors_length_hi        :4; 
#else  
   u_char transport_descriptors_length_hi        :4; 
   u_char                                        :4; 
#endif
   u_char transport_descriptors_length_lo        :8; 
   /* descriptors  */
} nit_ts_t;

/*
 *
 *    In addition to the PSI, data is needed to provide identification of
 *    services and events for the user. In contrast with the PAT, CAT, and
 *    PMT of the PSI, which give information only for the multiplex in which
 *    they are contained (the actual multiplex), the additional information
 *    defined within the present document can also provide information on
 *    services and events carried by different multiplexes, and even on other
 *    networks. This data is structured as nine tables:
 *
 *    1) Bouquet Association Table (BAT):
 *
 *       - the BAT provides information regarding bouquets. As well as giving
 *         the name of the bouquet, it provides a list of services for each
 *         bouquet.
 *
 */
/* SEE NIT (It has the same structure but has different allowed descriptors) */
/*
 *
 *    2) Service Description Table (SDT):
 *
 *       - the SDT contains data describing the services in the system e.g.
 *         names of services, the service provider, etc.
 *
 */

#define SDT_LEN 11

typedef struct {
   u_char table_id                               :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char                                        :3;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :3;
   u_char section_syntax_indicator               :1;
#endif
   u_char section_length_lo                      :8;
   u_char transport_stream_id_hi                 :8;
   u_char transport_stream_id_lo                 :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :2;
   u_char version_number                         :5;
   u_char current_next_indicator                 :1;
#else
   u_char current_next_indicator                 :1;
   u_char version_number                         :5;
   u_char                                        :2;
#endif
   u_char section_number                         :8;
   u_char last_section_number                    :8;
   u_char original_network_id_hi                 :8;
   u_char original_network_id_lo                 :8;
   u_char                                        :8;
} sdt_t;

#define GetSDTTransportStreamId(x) (HILO(((sdt_t *) x)->transport_stream_id))
#define GetSDTOriginalNetworkId(x) (HILO(((sdt_t *) x)->original_network_id))

#define SDT_DESCR_LEN 5

typedef struct {
   u_char service_id_hi                          :8;
   u_char service_id_lo                          :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :6;
   u_char eit_schedule_flag                      :1;
   u_char eit_present_following_flag             :1;
   u_char running_status                         :3;
   u_char free_ca_mode                           :1;
   u_char descriptors_loop_length_hi             :4;
#else
   u_char eit_present_following_flag             :1;
   u_char eit_schedule_flag                      :1;
   u_char                                        :6;
   u_char descriptors_loop_length_hi             :4;
   u_char free_ca_mode                           :1;
   u_char running_status                         :3;
#endif
   u_char descriptors_loop_length_lo             :8;
} sdt_descr_t;

/*
 *
 *    3) Event Information Table (EIT):
 * 
 *       - the EIT contains data concerning events or programmes such as event
 *         name, start time, duration, etc.; - the use of different descriptors
 *         allows the transmission of different kinds of event information e.g.
 *         for different service types.
 *
 */

#define EIT_LEN 14

typedef struct {
   u_char table_id                               :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char                                        :3;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :3;
   u_char section_syntax_indicator               :1;
#endif
   u_char section_length_lo                      :8;
   u_char service_id_hi                          :8;
   u_char service_id_lo                          :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :2;
   u_char version_number                         :5;
   u_char current_next_indicator                 :1;
#else
   u_char current_next_indicator                 :1;
   u_char version_number                         :5;
   u_char                                        :2;
#endif
   u_char section_number                         :8;
   u_char last_section_number                    :8;
   u_char transport_stream_id_hi                 :8;
   u_char transport_stream_id_lo                 :8;
   u_char original_network_id_hi                 :8;
   u_char original_network_id_lo                 :8;
   u_char segment_last_section_number            :8;
   u_char segment_last_table_id                  :8;
} eit_t;

#define EIT_EVENT_LEN 12

typedef struct {
   u_char event_id_hi                            :8;
   u_char event_id_lo                            :8;
   u_char mjd_hi                                 :8;
   u_char mjd_lo                                 :8;
   u_char start_time_h                           :8;
   u_char start_time_m                           :8;
   u_char start_time_s                           :8;
   u_char duration_h                             :8;
   u_char duration_m                             :8;
   u_char duration_s                             :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char running_status                         :3;
   u_char free_ca_mode                           :1;
   u_char descriptors_loop_length_hi             :4;
#else
   u_char descriptors_loop_length_hi             :4;
   u_char free_ca_mode                           :1;
   u_char running_status                         :3;
#endif
   u_char descriptors_loop_length_lo             :8;
} eit_event_t;

/*
 *
 *    4) Running Status Table (RST):
 *
 *       - the RST gives the status of an event (running/not running). The RST
 *         updates this information and allows timely automatic switching to
 *         events.
 *
 */
    /* TO BE DONE */
/*
 *
 *    5) Time and Date Table (TDT):
 *
 *       - the TDT gives information relating to the present time and date.
 *         This information is given in a separate table due to the frequent
 *         updating of this information.
 *
 */

#define TDT_LEN 8

typedef struct {
   u_char table_id                               :8; 
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char                                        :3;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :3;
   u_char section_syntax_indicator               :1;
#endif
   u_char section_length_lo                      :8;
   u_char utc_mjd_hi                             :8;
   u_char utc_mjd_lo                             :8;
   u_char utc_time_h                             :8;
   u_char utc_time_m                             :8;
   u_char utc_time_s                             :8;
} tdt_t;

/*
 *
 *    6) Time Offset Table (TOT):
 *
 *       - the TOT gives information relating to the present time and date and
 *         local time offset. This information is given in a separate table due
 *         to the frequent updating of the time information.
 *
 */
#define TOT_LEN 10

typedef struct {
   u_char table_id                               :8; 
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char                                        :3;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :3;
   u_char section_syntax_indicator               :1;
#endif
   u_char section_length_lo                      :8;
   u_char utc_mjd_hi                             :8;
   u_char utc_mjd_lo                             :8;
   u_char utc_time_h                             :8;
   u_char utc_time_m                             :8;
   u_char utc_time_s                             :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :4;
   u_char descriptors_loop_length_hi             :4;
#else
   u_char descriptors_loop_length_hi             :4;
   u_char                                        :4;
#endif
   u_char descriptors_loop_length_lo             :8;
} tot_t;


/*
 *
 *    7) Stuffing Table (ST):
 *
 *       - the ST is used to invalidate existing sections, for example at
 *         delivery system boundaries.
 *
 */
    /* TO BE DONE */
/*
 *
 *    8) Selection Information Table (SIT):
 *
 *       - the SIT is used only in "partial" (i.e. recorded) bitstreams. It
 *         carries a summary of the SI information required to describe the
 *         streams in the partial bitstream.
 *
 */
    /* TO BE DONE */
/*
 *
 *    9) Discontinuity Information Table (DIT):
 *
 *       - the DIT is used only in "partial" (i.e. recorded) bitstreams.
 *         It is inserted where the SI information in the partial bitstream may
 *         be discontinuous. Where applicable the use of descriptors allows a
 *         flexible approach to the organization of the tables and allows for
 *         future compatible extensions.
 *
 */
    /* TO BE DONE */
/*
 *
 *    The following describes the different descriptors that can be used within
 *    the SI.
 *
 *    The following semantics apply to all the descriptors defined in this
 *    subclause:
 *
 *    descriptor_tag: The descriptor tag is an 8-bit field which identifies
 *                    each descriptor. Those values with MPEG-2 normative
 *                    meaning are described in ISO/IEC 13818-1. The values of
 *                    descriptor_tag are defined in 'libsi.h'
 *    descriptor_length: The descriptor length is an 8-bit field specifying the
 *                       total number of bytes of the data portion of the
 *                       descriptor following the byte defining the value of
 *                       this field.
 *
 */

#define DESCR_GEN_LEN 2
typedef struct descr_gen_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_gen_t;
#define CastGenericDescriptor(x) ((descr_gen_t *)(x))

#define GetDescriptorTag(x) (((descr_gen_t *) x)->descriptor_tag)
#define GetDescriptorLength(x) (((descr_gen_t *) x)->descriptor_length+DESCR_GEN_LEN)


/* 0x09 ca_descriptor */

#define DESCR_CA_LEN 6
typedef struct descr_ca_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char CA_type_hi                             :8;
   u_char CA_type_lo                             :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char reserved                               :3;
   u_char CA_PID_hi                              :5;
#else
   u_char CA_PID_hi                              :5;
   u_char reserved                               :3;
#endif
   u_char CA_PID_lo                              :8;
} descr_ca_t;
#define CastCaDescriptor(x) ((descr_ca_t *)(x))

/* 0x0A iso_639_language_descriptor */

#define DESCR_ISO_639_LANGUAGE_LEN 5
typedef struct descr_iso_639_language_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char lang_code1                             :8;
   u_char lang_code2                             :8;
   u_char lang_code3                             :8;
} descr_iso_639_language_t;
#define CastIso639LanguageDescriptor(x) ((descr_iso_639_language_t *)(x))


/* 0x40 network_name_descriptor */

#define DESCR_NETWORK_NAME_LEN 2
typedef struct descr_network_name_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_network_name_t;
#define CastNetworkNameDescriptor(x) ((descr_network_name_t *)(x))


/* 0x41 service_list_descriptor */

#define DESCR_SERVICE_LIST_LEN 2
typedef struct descr_service_list_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_service_list_t;
#define CastServiceListDescriptor(x) ((descr_service_list_t *)(x))

#define DESCR_SERVICE_LIST_LOOP_LEN 3
typedef struct descr_service_list_loop_struct {
   u_char service_id_hi                          :8;
   u_char service_id_lo                          :8;
   u_char service_type                           :8;
} descr_service_list_loop_t;
#define CastServiceListDescriptorLoop(x) ((descr_service_list_loop_t *)(x))


/* 0x42 stuffing_descriptor */

#define DESCR_STUFFING_LEN XX
typedef struct descr_stuffing_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_stuffing_t;
#define CastStuffingDescriptor(x) ((descr_stuffing_t *)(x))


/* 0x43 satellite_delivery_system_descriptor */

#define DESCR_SATELLITE_DELIVERY_SYSTEM_LEN 13
typedef struct descr_satellite_delivery_system_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char frequency1                             :8;
   u_char frequency2                             :8;
   u_char frequency3                             :8;
   u_char frequency4                             :8;
   u_char orbital_position1                      :8;
   u_char orbital_position2                      :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char west_east_flag                         :1;
   u_char polarization                           :2;
   u_char modulation                             :5;
#else
   u_char modulation                             :5;
   u_char polarization                           :2;
   u_char west_east_flag                         :1;
#endif
   u_char symbol_rate1                           :8;
   u_char symbol_rate2                           :8;
   u_char symbol_rate3                           :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char symbol_rate4                           :4;
   u_char fec_inner                              :4;
#else
   u_char fec_inner                              :4;
   u_char symbol_rate4                           :4;
#endif
} descr_satellite_delivery_system_t;
#define CastSatelliteDeliverySystemDescriptor(x) ((descr_satellite_delivery_system_t *)(x))


/* 0x44 cable_delivery_system_descriptor */

#define DESCR_CABLE_DELIVERY_SYSTEM_LEN 13
typedef struct descr_cable_delivery_system_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char frequency1                             :8;
   u_char frequency2                             :8;
   u_char frequency3                             :8;
   u_char frequency4                             :8;
   u_char reserved1                              :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char reserved2                              :4;
   u_char fec_outer                              :4;
#else
   u_char fec_outer                              :4;
   u_char reserved2                              :4;
#endif
   u_char modulation                             :8;
   u_char symbol_rate1                           :8;
   u_char symbol_rate2                           :8;
   u_char symbol_rate3                           :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char symbol_rate4                           :4;
   u_char fec_inner                              :4;
#else
   u_char fec_inner                              :4;
   u_char symbol_rate4                           :4;
#endif
} descr_cable_delivery_system_t;
#define CastCableDeliverySystemDescriptor(x) ((descr_cable_delivery_system_t *)(x))


/* 0x45 vbi_data_descriptor */

#define DESCR_VBI_DATA_LEN XX
typedef struct descr_vbi_data_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_vbi_data_t;
#define CastVbiDataDescriptor(x) ((descr_vbi_data_t *)(x))


/* 0x46 vbi_teletext_descriptor */

#define DESCR_VBI_TELETEXT_LEN XX
typedef struct descr_vbi_teletext_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_vbi_teletext_t;
#define CastVbiDescriptor(x) ((descr_vbi_teletext_t *)(x))


/* 0x47 bouquet_name_descriptor */

#define DESCR_BOUQUET_NAME_LEN 2
typedef struct descr_bouquet_name_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_bouquet_name_t;
#define CastBouquetNameDescriptor(x) ((descr_bouquet_name_t *)(x))


/* 0x48 service_descriptor */

#define DESCR_SERVICE_LEN  4
typedef struct descr_service_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char service_type                           :8;
   u_char provider_name_length                   :8;
} descr_service_t;
#define CastServiceDescriptor(x) ((descr_service_t *)(x))


/* 0x49 country_availability_descriptor */

#define DESCR_COUNTRY_AVAILABILITY_LEN 3
typedef struct descr_country_availability_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char country_availability_flag              :1;
   u_char reserved                               :7;
#else
   u_char reserved                               :7;
   u_char country_availability_flag              :1;
#endif
} descr_country_availability_t;
#define CastCountryAvailabilityDescriptor(x) ((descr_country_availability_t *)(x))


/* 0x4A linkage_descriptor */

#define DESCR_LINKAGE_LEN 9
typedef struct descr_linkage_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char transport_stream_id_hi                 :8;
   u_char transport_stream_id_lo                 :8;
   u_char original_network_id_hi                 :8; 
   u_char original_network_id_lo                 :8; 
   u_char service_id_hi                          :8;
   u_char service_id_lo                          :8;
   u_char linkage_type                           :8;
} descr_linkage_t;
#define CastLinkageDescriptor(x) ((descr_linkage_t *)(x))


/* 0x4B nvod_reference_descriptor */

#define DESCR_NVOD_REFERENCE_LEN 2
typedef struct descr_nvod_reference_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_nvod_reference_t;
#define CastNvodReferenceDescriptor(x) ((descr_nvod_reference_t *)(x))

#define ITEM_NVOD_REFERENCE_LEN 6
typedef struct item_nvod_reference_struct {
   u_char transport_stream_id_hi                 :8;
   u_char transport_stream_id_lo                 :8;
   u_char original_network_id_hi                 :8; 
   u_char original_network_id_lo                 :8; 
   u_char service_id_hi                          :8;
   u_char service_id_lo                          :8;
} item_nvod_reference_t;
#define CastNvodReferenceItem(x) ((item_nvod_reference_t *)(x))



/* 0x4C time_shifted_service_descriptor */

#define DESCR_TIME_SHIFTED_SERVICE_LEN 4
typedef struct descr_time_shifted_service_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char reference_service_id_hi                :8;
   u_char reference_service_id_lo                :8;
} descr_time_shifted_service_t;
#define CastTimeShiftedServiceDescriptor(x) ((descr_time_shifted_service_t *)(x))


/* 0x4D short_event_descriptor */

#define DESCR_SHORT_EVENT_LEN 6
typedef struct descr_short_event_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char lang_code1                             :8;
   u_char lang_code2                             :8;
   u_char lang_code3                             :8;
   u_char event_name_length                      :8;
} descr_short_event_t;
#define CastShortEventDescriptor(x) ((descr_short_event_t *)(x))


/* 0x4E extended_event_descriptor */

#define DESCR_EXTENDED_EVENT_LEN 7
typedef struct descr_extended_event_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
#if BYTE_ORDER == BIG_ENDIAN
   u_char descriptor_number                      :4;
   u_char last_descriptor_number                 :4;
#else
   u_char last_descriptor_number                 :4;
   u_char descriptor_number                      :4;
#endif
   u_char lang_code1                             :8;
   u_char lang_code2                             :8;
   u_char lang_code3                             :8;
   u_char length_of_items                        :8;
} descr_extended_event_t;
#define CastExtendedEventDescriptor(x) ((descr_extended_event_t *)(x))

#define ITEM_EXTENDED_EVENT_LEN 1
typedef struct item_extended_event_struct {
   u_char item_description_length               :8;
} item_extended_event_t;
#define CastExtendedEventItem(x) ((item_extended_event_t *)(x))


/* 0x4F time_shifted_event_descriptor */

#define DESCR_TIME_SHIFTED_EVENT_LEN 6
typedef struct descr_time_shifted_event_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char reference_service_id_hi                :8;
   u_char reference_service_id_lo                :8;
   u_char reference_event_id_hi                  :8;
   u_char reference_event_id_lo                  :8;
} descr_time_shifted_event_t;
#define CastTimeShiftedEventDescriptor(x) ((descr_time_shifted_event_t *)(x))


/* 0x50 component_descriptor */

#define DESCR_COMPONENT_LEN  8
typedef struct descr_component_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char reserved                               :4;
   u_char stream_content                         :4;
#else
   u_char stream_content                         :4;
   u_char reserved                               :4;
#endif
   u_char component_type                         :8;
   u_char component_tag                          :8;
   u_char lang_code1                             :8;
   u_char lang_code2                             :8;
   u_char lang_code3                             :8;
} descr_component_t;
#define CastComponentDescriptor(x) ((descr_component_t *)(x))


/* 0x51 mosaic_descriptor */

#define DESCR_MOSAIC_LEN XX
typedef struct descr_mosaic_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_mosaic_t;
#define CastMosaicDescriptor(x) ((descr_mosaic_t *)(x))


/* 0x52 stream_identifier_descriptor */

#define DESCR_STREAM_IDENTIFIER_LEN 3
typedef struct descr_stream_identifier_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char component_tag                          :8;
} descr_stream_identifier_t;
#define CastStreamIdentifierDescriptor(x) ((descr_stream_identifier_t *)(x))


/* 0x53 ca_identifier_descriptor */

#define DESCR_CA_IDENTIFIER_LEN 2
typedef struct descr_ca_identifier_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_ca_identifier_t;
#define CastCaIdentifierDescriptor(x) ((descr_ca_identifier_t *)(x))


/* 0x54 content_descriptor */

#define DESCR_CONTENT_LEN 2
typedef struct descr_content_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_content_t;
#define CastContentDescriptor(x) ((descr_content_t *)(x))

typedef struct nibble_content_struct {
#if BYTE_ORDER == BIG_ENDIAN
   u_char content_nibble_level_1                 :4;
   u_char content_nibble_level_2                 :4;
   u_char user_nibble_1                          :4;
   u_char user_nibble_2                          :4;
#else
   u_char user_nibble_2                          :4;
   u_char user_nibble_1                          :4;
   u_char content_nibble_level_2                 :4;
   u_char content_nibble_level_1                 :4;
#endif
} nibble_content_t;
#define CastContentNibble(x) ((nibble_content_t *)(x))


/* 0x55 parental_rating_descriptor */

#define DESCR_PARENTAL_RATING_LEN 2
typedef struct descr_parental_rating_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_parental_rating_t;
#define CastParentalRatingDescriptor(x) ((descr_parental_rating_t *)(x))

#define PARENTAL_RATING_LEN 4
typedef struct parental_rating_struct {
   u_char lang_code1                             :8;
   u_char lang_code2                             :8;
   u_char lang_code3                             :8;
   u_char rating                                 :8;
} parental_rating_t;
#define CastParentalRating(x) ((parental_rating_t *)(x))


/* 0x56 teletext_descriptor */

#define DESCR_TELETEXT_LEN 2
typedef struct descr_teletext_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_teletext_t;
#define CastTeletextDescriptor(x) ((descr_teletext_t *)(x))

#define ITEM_TELETEXT_LEN 5
typedef struct item_teletext_struct {
   u_char lang_code1                             :8;
   u_char lang_code2                             :8;
   u_char lang_code3                             :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char type                                   :5; 
   u_char magazine_number                        :3; 
#else 
   u_char magazine_number                        :3; 
   u_char type                                   :5; 
#endif 
   u_char page_number                            :8; 
} item_teletext_t;
#define CastTeletextItem(x) ((item_teletext_t *)(x))


/* 0x57 telephone_descriptor */

#define DESCR_TELEPHONE_LEN XX
typedef struct descr_telephone_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_telephone_t;
#define CastTelephoneDescriptor(x) ((descr_telephone_t *)(x))


/* 0x58 local_time_offset_descriptor */

#define DESCR_LOCAL_TIME_OFFSET_LEN 2
typedef struct descr_local_time_offset_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_local_time_offset_t;
#define CastLocalTimeOffsetDescriptor(x) ((descr_local_time_offset_t *)(x))

#define LOCAL_TIME_OFFSET_ENTRY_LEN 15
typedef struct local_time_offset_entry_struct {
   u_char country_code1                          :8;
   u_char country_code2                          :8;
   u_char country_code3                          :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char country_region_id                      :6;
   u_char                                        :1;
   u_char local_time_offset_polarity             :1;
#else
   u_char local_time_offset_polarity             :1;
   u_char                                        :1;
   u_char country_region_id                      :6;
#endif
   u_char local_time_offset_h                    :8;
   u_char local_time_offset_m                    :8;
   u_char time_of_change_mjd_hi                  :8;
   u_char time_of_change_mjd_lo                  :8;
   u_char time_of_change_time_h                  :8;
   u_char time_of_change_time_m                  :8;
   u_char time_of_change_time_s                  :8;
   u_char next_time_offset_h                     :8;
   u_char next_time_offset_m                     :8;
} local_time_offset_entry_t ;
#define CastLocalTimeOffsetEntry(x) ((local_time_offset_entry_t *)(x))


/* 0x59 subtitling_descriptor */

#define DESCR_SUBTITLING_LEN 2
typedef struct descr_subtitling_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
} descr_subtitling_t;
#define CastSubtitlingDescriptor(x) ((descr_subtitling_t *)(x))

#define ITEM_SUBTITLING_LEN 8
typedef struct item_subtitling_struct {
   u_char lang_code1                             :8;
   u_char lang_code2                             :8;
   u_char lang_code3                             :8;
   u_char subtitling_type                        :8; 
   u_char composition_page_id_hi                 :8; 
   u_char composition_page_id_lo                 :8; 
   u_char ancillary_page_id_hi                   :8; 
   u_char ancillary_page_id_lo                   :8; 
} item_subtitling_t;
#define CastSubtitlingItem(x) ((item_subtitling_t *)(x))


/* 0x5A terrestrial_delivery_system_descriptor */

#define DESCR_TERRESTRIAL_DELIVERY_SYSTEM_LEN XX
typedef struct descr_terrestrial_delivery_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char frequency1                             :8;
   u_char frequency2                             :8;
   u_char frequency3                             :8;
   u_char frequency4                             :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char bandwidth                              :3;
   u_char reserved1                              :5;
#else
   u_char reserved1                              :5;
   u_char bandwidth                              :3;
#endif
#if BYTE_ORDER == BIG_ENDIAN
   u_char constellation                          :2;
   u_char hierarchy                              :3;
   u_char code_rate_HP                           :3;
#else
   u_char code_rate_HP                           :3;
   u_char hierarchy                              :3;
   u_char constellation                          :2;
#endif
#if BYTE_ORDER == BIG_ENDIAN
   u_char code_rate_LP                           :3;
   u_char guard_interval                         :2;
   u_char transmission_mode                      :2;
   u_char other_frequency_flag                   :1;
#else
   u_char other_frequency_flag                   :1;
   u_char transmission_mode                      :2;
   u_char guard_interval                         :2;
   u_char code_rate_LP                           :3;
#endif
   u_char reserver2                              :8;
   u_char reserver3                              :8;
   u_char reserver4                              :8;
   u_char reserver5                              :8;
} descr_terrestrial_delivery_system_t;
#define CastTerrestrialDeliverySystemDescriptor(x) ((descr_terrestrial_delivery_system_t *)(x))


/* 0x5B multilingual_network_name_descriptor */

#define DESCR_MULTILINGUAL_NETWORK_NAME_LEN XX
typedef struct descr_multilingual_network_name_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_multilingual_network_name_t;
#define CastMultilingualNetworkNameDescriptor(x) ((descr_multilingual_network_name_t *)(x))


/* 0x5C multilingual_bouquet_name_descriptor */

#define DESCR_MULTILINGUAL_BOUQUET_NAME_LEN XX
typedef struct descr_multilingual_bouquet_name_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_multilingual_bouquet_name_t;
#define CastMultilingualBouquetNameDescriptor(x) ((descr_multilingual_bouquet_name_t *)(x))


/* 0x5D multilingual_service_name_descriptor */

#define DESCR_MULTILINGUAL_SERVICE_NAME_LEN XX
typedef struct descr_multilingual_service_name_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_multilingual_service_name_t;
#define CastMultilingualServiceNameDescriptor(x) ((descr_multilingual_service_name_t *)(x))


/* 0x5E multilingual_component_descriptor */

#define DESCR_MULTILINGUAL_COMPONENT_LEN XX
typedef struct descr_multilingual_component_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_multilingual_component_t;
#define CastMultilingualComponentDescriptor(x) ((descr_multilingual_component_t *)(x))


/* 0x5F private_data_specifier_descriptor */

#define DESCR_PRIVATE_DATA_SPECIFIER_LEN XX
typedef struct descr_private_data_specifier_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_private_data_specifier_t;
#define CastPrivateDataSpecifierDescriptor(x) ((descr_private_data_specifier_t *)(x))


/* 0x60 service_move_descriptor */

#define DESCR_SERVICE_MOVE_LEN XX
typedef struct descr_service_move_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_service_move_t;
#define CastServiceMoveDescriptor(x) ((descr_service_move_t *)(x))


/* 0x61 short_smoothing_buffer_descriptor */

#define DESCR_SHORT_SMOOTHING_BUFFER_LEN XX
typedef struct descr_short_smoothing_buffer_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_short_smoothing_buffer_t;
#define CastShortSmoothingBufferDescriptor(x) ((descr_short_smoothing_buffer_t *)(x))


/* 0x62 frequency_list_descriptor */

#define DESCR_FREQUENCY_LIST_LEN XX
typedef struct descr_frequency_list_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_frequency_list_t;
#define CastFrequencyListDescriptor(x) ((descr_frequency_list_t *)(x))


/* 0x63 partial_transport_stream_descriptor */

#define DESCR_PARTIAL_TRANSPORT_STREAM_LEN XX
typedef struct descr_partial_transport_stream_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_partial_transport_stream_t;
#define CastPartialDescriptor(x) ((descr_partial_transport_stream_t *)(x))


/* 0x64 data_broadcast_descriptor */

#define DESCR_DATA_BROADCAST_LEN XX
typedef struct descr_data_broadcast_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_data_broadcast_t;
#define CastDataBroadcastDescriptor(x) ((descr_data_broadcast_t *)(x))


/* 0x65 ca_system_descriptor */

#define DESCR_CA_SYSTEM_LEN XX
typedef struct descr_ca_system_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_ca_system_t;
#define CastCaSystemDescriptor(x) ((descr_ca_system_t *)(x))


/* 0x66 data_broadcast_id_descriptor */

#define DESCR_DATA_BROADCAST_ID_LEN XX
typedef struct descr_data_broadcast_id_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_data_broadcast_id_t;
#define CastDataBroadcastIdDescriptor(x) ((descr_data_broadcast_id_t *)(x))


/* 0x67 transport_stream_descriptor */

#define DESCR_TRANSPORT_STREAM_LEN XX
typedef struct descr_transport_stream_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_transport_stream_t;
#define CastTransportStreamDescriptor(x) ((descr_transport_stream_t *)(x))


/* 0x68 dsng_descriptor */

#define DESCR_DSNG_LEN XX
typedef struct descr_dsng_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_dsng_t;
#define CastDsngDescriptor(x) ((descr_dsng_t *)(x))


/* 0x69 pdc_descriptor */

#define DESCR_PDC_LEN XX
typedef struct descr_pdc_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_pdc_t;
#define CastPdcDescriptor(x) ((descr_pdc_t *)(x))


/* 0x6A ac3_descriptor */

#define DESCR_AC3_LEN 3
typedef struct descr_ac3_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char ac3_type_flag                          :1;
   u_char bsid_flag                              :1;
   u_char mainid_flag                            :1;
   u_char asvc_flag                              :1;
   u_char reserved                               :4;
#else
   u_char reserved                               :4;
   u_char asvc_flag                              :1;
   u_char mainid_flag                            :1;
   u_char bsid_flag                              :1;
   u_char ac3_type_flag                          :1;
#endif
   u_char ac3_type                               :8;
   u_char bsid                                   :8;
   u_char mainid                                 :8;
   u_char asvc                                   :8;
} descr_ac3_t;
#define CastAc3Descriptor(x) ((descr_ac3_t *)(x))


/* 0x6B ancillary_data_descriptor */

#define DESCR_ANCILLARY_DATA_LEN 3
typedef struct descr_ancillary_data_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   u_char ancillary_data_identifier              :8;
} descr_ancillary_data_t;
#define CastAncillaryDataDescriptor(x) ((descr_ancillary_data_t *)(x))


/* 0x6C cell_list_descriptor */

#define DESCR_CELL_LIST_LEN XX
typedef struct descr_cell_list_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_cell_list_t;
#define CastCellListDescriptor(x) ((descr_cell_list_t *)(x))


/* 0x6D cell_frequency_link_descriptor */

#define DESCR_CELL_FREQUENCY_LINK_LEN XX
typedef struct descr_cell_frequency_link_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_cell_frequency_link_t;
#define CastCellFrequencyLinkDescriptor(x) ((descr_cell_frequency_link_t *)(x))


/* 0x6E announcement_support_descriptor */

#define DESCR_ANNOUNCEMENT_SUPPORT_LEN XX
typedef struct descr_announcement_support_struct {
   u_char descriptor_tag                         :8;
   u_char descriptor_length                      :8;
   /* TBD */
} descr_announcement_support_t;
#define CastAnnouncementSupportDescriptor(x) ((descr_announcement_support_t *)(x))


