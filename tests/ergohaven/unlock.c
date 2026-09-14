static void reset(uint32_t at) { mock_now=at; holding=false; vial_unlocked=0; command(vial_unlock_start); }
static void scan(bool down) { holding=down; vial_unlock_task(); }
static void advance(unsigned duration, unsigned cadence) {
    for(unsigned i=0;i<duration;i++) { mock_now++; vial_unlock_task(); if(cadence && i%cadence==0) command(vial_unlock_poll); }
}
int main(void) {
    reset(0); advance(10000,0); scan(true); command(vial_unlock_poll); assert(!vial_unlocked);
    advance(2999,0); assert(!vial_unlocked && vial_unlock_counter==1); advance(1,0); assert(vial_unlocked);
    unsigned cadences[]={0,1,10,100,333,1000,4000};
    for(unsigned i=0;i<sizeof(cadences)/sizeof(cadences[0]);i++) {
        reset(0); scan(true); advance(2999,cadences[i]); assert(!vial_unlocked);
        advance(1,cadences[i]); assert(vial_unlocked && !vial_unlock_in_progress);
        reset(0); scan(true); advance(2900,cadences[i]); scan(false); advance(1,0); scan(true);
        command(vial_unlock_poll); assert(!vial_unlocked && vial_unlock_counter==30);
        advance(2999,cadences[i]); assert(!vial_unlocked); advance(1,cadences[i]); assert(vial_unlocked);
    }
    reset(UINT32_MAX-1500); scan(true); advance(2999,1); assert(!vial_unlocked); advance(1,1); assert(vial_unlocked);
    reset(0); scan(true); advance(2900,1); command(vial_unlock_start); scan(true); advance(2999,1); assert(!vial_unlocked); advance(1,1); assert(vial_unlocked);
    reset(0); scan(true); advance(2900,1); command(vial_lock); advance(5000,1); assert(!vial_unlocked && !vial_unlock_in_progress);
    reset(0); mock_now=100000; holding=true; command(vial_unlock_poll); assert(!vial_unlocked); scan(true); assert(!vial_unlocked);
    puts("unlock: exact 3000ms continuous hold, 1ms release, seven poll cadences incl absent, restart/lock cancellation, unsampled time and 32-bit wrap: PASS");
}
