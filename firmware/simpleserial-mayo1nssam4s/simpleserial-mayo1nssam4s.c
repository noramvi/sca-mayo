#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "hal.h"
#include "simpleserial.h"

#include "api.h"
#include "arithmetic.h"
#include "arithmetic_fixed.h"
#include "mayo.h"
#include "sam4s.h"
#include "p1p1t_data.h"


#define LIMBS 5
#define NUM_ENTRIES 77
#define MAX_R0_ENTRIES 77
uint64_t P1_r0[MAX_R0_ENTRIES][LIMBS];
static int p1_offset = 0;
//static uint64_t P_buffer[NUM_ENTRIES][LIMBS];
static uint8_t target_oil_coeff = 0;
//static uint64_t P_buffer[NUM_ENTRIES][LIMBS];
static uint32_t P_offset = 0; 


static uint64_t P_buffer[NUM_ENTRIES * LIMBS];
static uint64_t acc_init_buffer[NUM_ENTRIES * LIMBS];
static uint64_t acc_out_buffer[NUM_ENTRIES * LIMBS];
static uint64_t acc_work_buffer[NUM_ENTRIES * LIMBS];
static size_t acc_out_read_offset = 0;

uint8_t test_cmd(uint8_t cmd, uint8_t scmd,
                 uint8_t len, uint8_t *buf)
{
    (void)cmd;
    (void)scmd;
    (void)buf;
    (void)len;

    trigger_high();
    for (volatile uint32_t i = 0; i < 200000; i++);
    trigger_low();

    uint8_t ok = 0x99;
    simpleserial_put('r', 1, &ok);
    

    return SS_ERR_OK;
}

static int acc_offset = 0;


uint8_t load_Pentries(uint8_t cmd,
                          uint8_t scmd,
                          uint8_t len,
                          uint8_t *buf)
{
    if (P_offset == 0) {
        memset(P_buffer, 0, sizeof(P_buffer));
    }

    memcpy((uint8_t*)P_buffer + P_offset, buf, len);
    P_offset += len;

    if (P_offset >= sizeof(P_buffer)) {
        P_offset = 0; 
    }

    return SS_ERR_OK;
}

uint8_t set_target_o(uint8_t cmd,
                         uint8_t scmd,
                         uint8_t len,
                         uint8_t *buf)
{
    if (len != 1)
        return SS_ERR_LEN;

    target_oil_coeff = buf[0] & 0x0F; 

    return SS_ERR_OK;
}


uint8_t cmd_get_O(uint8_t cmd,
                  uint8_t scmd,
                  uint8_t len,
                  uint8_t *buf)
{
    (void)cmd;
    (void)scmd;
    (void)len;
    (void)buf;

    simpleserial_put('r', 1, &target_oil_coeff);

    return SS_ERR_OK;
}



uint8_t cmd_start(uint8_t cmd,
                  uint8_t scmd,
                  uint8_t len,
                  uint8_t *buf)
{
    (void)cmd;
    (void)scmd;
    (void)len;
    (void)buf;

    P_offset = 0;
    acc_out_read_offset = 0;

    memset(P_buffer, 0, sizeof(P_buffer));
    memset(acc_init_buffer, 0, sizeof(acc_init_buffer));
    memset(acc_out_buffer, 0, sizeof(acc_out_buffer));
    memset(acc_work_buffer, 0, sizeof(acc_work_buffer));

    return SS_ERR_OK;
}

uint8_t load_acc_init_rows(uint8_t cmd,
                               uint8_t scmd,
                               uint8_t len,
                               uint8_t *buf)
{
    static uint32_t acc_init_offset = 0;

    if (acc_init_offset == 0) {
        memset(acc_init_buffer, 0, sizeof(acc_init_buffer));
    }

    memcpy((uint8_t*)acc_init_buffer + acc_init_offset, buf, len);
    acc_init_offset += len;

    if (acc_init_offset >= sizeof(acc_init_buffer)) {
        acc_init_offset = 0;
    }

    return SS_ERR_OK;
}

uint8_t attack_one_coeff(uint8_t cmd,
                                   uint8_t scmd,
                                   uint8_t len,
                                   uint8_t *buf)
{
    memcpy(acc_work_buffer,
           acc_init_buffer,
           sizeof(acc_init_buffer));

    trigger_high();

    for (int i = 0; i < NUM_ENTRIES; i++) {
        m_vec_mul_add(
            LIMBS,
            &P_buffer[i * LIMBS],
            target_oil_coeff,
            &acc_work_buffer[i * LIMBS]
        );
    }

    trigger_low();

    memcpy(acc_out_buffer,
           acc_work_buffer,
           sizeof(acc_out_buffer));

    return SS_ERR_OK;
}

uint8_t read_acc_out_rows(uint8_t cmd,
                              uint8_t scmd,
                              uint8_t len,
                              uint8_t *buf)
{
    if (len < 1) {
        return SS_ERR_OK;
    }

    uint8_t out_len = buf[0];

    size_t remaining = sizeof(acc_out_buffer) - acc_out_read_offset;
    if (out_len > remaining) {
        out_len = (uint8_t)remaining;
    }

    simpleserial_put('r',
                     out_len,
                     ((uint8_t*)acc_out_buffer) + acc_out_read_offset);

    acc_out_read_offset += out_len;

    if (acc_out_read_offset >= sizeof(acc_out_buffer)) {
        acc_out_read_offset = 0;
    }

    return SS_ERR_OK;
}


uint8_t reset_acc_out_read(uint8_t cmd,
                               uint8_t scmd,
                               uint8_t len,
                               uint8_t *buf)
{
    acc_out_read_offset = 0;
    return SS_ERR_OK;
}


int main(void)
{
    platform_init();
    init_uart();
    trigger_setup();

    simpleserial_init();

    simpleserial_addcmd('t', 0, test_cmd);
    simpleserial_addcmd('o', 0, set_target_o);
    simpleserial_addcmd('s', 0, cmd_start);
    simpleserial_addcmd('h', 0, cmd_get_O);
    simpleserial_addcmd('c', 0, load_Pentries);
    simpleserial_addcmd('i', 0, load_acc_init_rows);   
    simpleserial_addcmd('a', 0, attack_one_coeff);
    simpleserial_addcmd('g', 0, read_acc_out_rows);
    simpleserial_addcmd('z', 0, reset_acc_out_read);



    while (1) {
        simpleserial_get();
    }
}