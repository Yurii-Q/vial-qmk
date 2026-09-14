/* Included after real eh_pictograms.c by run_native.py. No production copies. */
static uint8_t original[4194304], interrupted[4194304];
static unsigned scenarios;
static void packet(uint8_t *p) { assert(eh_pictograms_process_hid(p,32)); }
static void stage(unsigned slot, bool present, uint8_t fill) {
    uint8_t record[EH_PICTOGRAM_RECORD_SIZE]; memset(record,fill,sizeof(record));
    uint8_t p[32]={0xc7}; p[1]=slot/256; write_u16(p+2,slot%256); p[4]=present;
    write_u32(p+5,crc32(record,sizeof(record))); packet(p); assert(!p[1]);
    for(unsigned offset=0,seq=0;offset<sizeof(record);offset+=29,seq++) {
        memset(p,0,32); p[0]=0xc8; write_u16(p+1,seq);
        memcpy(p+3,record+offset,MIN(29,sizeof(record)-offset)); packet(p); assert(!p[1]);
    }
}
static void commit(void) { uint8_t p[32]={0xc9}; packet(p); assert(!p[1]); }
static void reboot(void) { cut_after=0; eh_pictograms_init(); }
static const uint8_t *icon(unsigned slot) { return eh_pictogram_for_keycode(slot<256?QK_MACRO+slot:QK_TAP_DANCE+slot-256); }
static uint8_t fill_for(unsigned slot) { return slot%127+1; }
static void seed(unsigned version) {
    memset(mock_flash,0xa5,sizeof(mock_flash));
    memset(mock_flash+EH_PICTOGRAM_FLASH_OFFSET,255,EH_PICTOGRAM_FLASH_SIZE);
    eh_pictogram_header_t *h=(void*)(mock_flash+EH_PICTOGRAM_FLASH_OFFSET);
    memset(h,0,sizeof(*h)); h->magic=EH_PICTOGRAM_MAGIC; h->version=version;
    h->width=h->height=version==3?32:35; h->bytes_per_icon=version==3?128:EH_PICTOGRAM_BYTES;
    h->total_size=256+512*(h->bytes_per_icon+4);
    memset(h->macro_valid,255,32); memset(h->tap_dance_valid,255,32);
    for(unsigned i=0;i<512;i++) memset(mock_flash+EH_PICTOGRAM_FLASH_OFFSET+256+i*(h->bytes_per_icon+4),fill_for(i),h->bytes_per_icon+4);
    h->data_crc32=crc32(stored_payload,h->total_size-256); h->header_crc32=header_crc((const uint8_t*)h);
    reboot(); assert(valid); memcpy(original,mock_flash,sizeof(original));
}
static void preservation(unsigned edited, bool present) {
    assert(valid); assert(storage_is_valid());
    assert(!memcmp(mock_flash,original,EH_PICTOGRAM_FLASH_OFFSET));
    unsigned end=EH_PICTOGRAM_FLASH_OFFSET+EH_PICTOGRAM_FLASH_SIZE;
    assert(!memcmp(mock_flash+end,original+end,sizeof(mock_flash)-end));
    bool old=icon(edited) && icon(edited)[0]==fill_for(edited);
    for(unsigned i=0;i<512;i++) {
        const uint8_t *p=icon(i);
        if(i==edited && !old && !present) { assert(!p); continue; }
        assert(p);
        uint8_t fill=(i==edited && !old)?0xE3:fill_for(i);
        for(unsigned j=0;j<EH_PICTOGRAM_BYTES;j++) assert(p[j]==fill);
        assert(eh_pictogram_color_for_keycode(i<256?QK_MACRO+i:QK_TAP_DANCE+i-256)==((uint32_t)fill*0x010101));
    }
}
static void interrupted_updates(unsigned slot, bool present) {
    memcpy(mock_flash,original,sizeof(mock_flash)); reboot(); stage(slot,present,0xE3);
    flash_operations=0; commit(); unsigned operations=flash_operations; preservation(slot,present);
    for(volatile unsigned mode=0;mode<3;mode++) for(volatile unsigned op=1;op<=operations;op++) {
        memcpy(mock_flash,original,sizeof(mock_flash)); reboot(); stage(slot,present,0xE3);
        cut_mode=mode; flash_operations=0; cut_after=op;
        if(!setjmp(power_cut)) commit();
        reboot(); preservation(slot,present); reboot(); preservation(slot,present); scenarios++;
    }
}
static void interrupted_recovery(void) {
    memcpy(mock_flash,original,sizeof(mock_flash)); reboot(); stage(24,true,0xE3);
    /* Tear the first live-sector erase, after backup/descriptor publication. */
    cut_mode=2; cut_after=0; cut_live=true;
    if(!setjmp(power_cut)) commit();
    cut_live=false; cut_after=0; memcpy(interrupted,mock_flash,sizeof(interrupted));
    assert(slot_journal_armed((const void*)flash_data(SLOT_JOURNAL_OFFSET)));
    /* Persistent backup corruption fails closed, without erasing live assets. */
    uint32_t backup=EH_PICTOGRAM_FLASH_OFFSET+SLOT_JOURNAL_OFFSET+FLASH_SECTOR_SIZE;
    mock_flash[backup]^=1; eh_pictograms_init(); assert(!valid);
    stage(200,true,0xE3); flash_operations=0; uint8_t p[32]={0xc9}; packet(p);
    assert(p[1]==EH_PICTOGRAM_STATUS_FLASH_ERROR && !flash_operations);
    mock_flash[backup]^=1;
    flash_operations=0; reboot(); unsigned operations=flash_operations;
    for(volatile unsigned mode=0;mode<3;mode++) for(volatile unsigned op=1;op<=operations;op++) {
        memcpy(mock_flash,interrupted,sizeof(mock_flash)); flash_operations=0; cut_after=op; cut_mode=mode;
        if(!setjmp(power_cut)) eh_pictograms_init();
        reboot(); preservation(24,true); assert(icon(24)[0]==fill_for(24)); scenarios++;
    }
}
static void complete_upload(bool bad_crc) {
    uint8_t p[32]={0xc2}; write_u32(p+1,EH_PICTOGRAM_PACKAGE_SIZE);
    const uint8_t *package=original+EH_PICTOGRAM_FLASH_OFFSET;
    write_u32(p+5,crc32(package,EH_PICTOGRAM_PACKAGE_SIZE)^(bad_crc?1:0)); packet(p); assert(!p[1]);
    for(unsigned offset=0,seq=0;offset<EH_PICTOGRAM_PACKAGE_SIZE;offset+=29,seq++) {
        memset(p,0,32); p[0]=0xc3; write_u16(p+1,seq);
        memcpy(p+3,package+offset,MIN(29,EH_PICTOGRAM_PACKAGE_SIZE-offset)); packet(p); assert(!p[1]);
    }
    memset(p,0,32); p[0]=0xc4; packet(p);
    assert(p[1]==(bad_crc?EH_PICTOGRAM_STATUS_BAD_CRC:0));
    reboot(); assert(valid!=bad_crc);
    if(!bad_crc) assert(!memcmp(package,flash_data(0),EH_PICTOGRAM_PACKAGE_SIZE));
}
int main(void) {
    seed(4);
    interrupted_updates(0,true); interrupted_updates(24,true); interrupted_updates(200,true);
    interrupted_updates(511,true); interrupted_updates(24,false); interrupted_recovery();
    printf("pictograms %uMiB: %u before/after/torn operation and reboot/recovery cuts preserve all 511 unrelated slots and reserved regions: PASS\n",PICO_FLASH_SIZE_BYTES/1048576,scenarios);
    complete_upload(false); complete_upload(true); complete_upload(false);
    puts("pictograms complete v4 upload, CRC rejection, exact package readback: PASS");
    uint8_t bulk[32]={0xc2}; write_u32(bulk+1,EH_PICTOGRAM_PACKAGE_SIZE); packet(bulk); assert(!bulk[1]);
    bulk[0]=0xc5; packet(bulk); bulk[0]=0xc4; packet(bulk); assert(bulk[1]==EH_PICTOGRAM_STATUS_NOT_UPLOADING);
    stage(1,true,0xE3); uint8_t p[32]={0xc5}; packet(p); assert(!valid);
    p[0]=0xc9; packet(p); assert(p[1]==EH_PICTOGRAM_STATUS_NOT_UPLOADING); reboot(); assert(!valid);
    stage(0,true,0x11); commit(); reboot(); assert(icon(0)[0]==0x11); assert(!icon(1));
    puts("pictograms CLEAR cancels slot upload; fresh empty-store slot commit: PASS");
    seed(3); assert(eh_pictogram_stored_width()==32);
    for(unsigned i=0;i<512;i++) assert(icon(i) && icon(i)[0]==fill_for(i));
    memset(p,0,32); p[0]=0xc7; p[4]=1; packet(p); assert(p[1]==EH_PICTOGRAM_STATUS_BAD_FORMAT);
    reboot(); assert(!memcmp(original,mock_flash,sizeof(mock_flash))); assert(eh_pictogram_stored_width()==32);
    memset(p,0,32); p[0]=0xcb; packet(p); assert(!p[1] && p[2]==fill_for(0));
    puts("pictograms legacy v3 reboot/read preserved byte-for-byte; incompatible slot edit rejected without writes: PASS");
}
