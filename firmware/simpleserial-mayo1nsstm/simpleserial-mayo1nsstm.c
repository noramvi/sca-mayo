#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "hal.h"
#include "simpleserial.h"
#include "p1p1t_data.h"

#include "api.h"
#include "arithmetic.h"
#include "arithmetic_fixed.h"
#include "generic_arithmetic.h"
#include "stm32f4_hal.h"
#include "mayo.h"


#define V 78
#define OIL 8
#define LIMBS 5

extern const mayo_params_t MAYO_1;

static uint8_t  O_global[V * OIL];
static uint64_t acc_init_full_buffer[V * OIL * LIMBS];
static uint64_t acc_full_buffer[V * OIL * LIMBS];

static uint32_t O_offset = 0;
static uint32_t p2_full_offset = 0;
static size_t acc_full_read_offset = 0;


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

uint8_t load_O(uint8_t cmd,
                   uint8_t scmd,
                   uint8_t len,
                   uint8_t *buf)
{
    (void)cmd;
    (void)scmd;

    if (O_offset == 0) {
        memset(O_global, 0, sizeof(O_global));
    }

    memcpy(O_global + O_offset, buf, len);
    O_offset += len;

    if (O_offset >= sizeof(O_global)) {
        O_offset = 0;
    }

    return SS_ERR_OK;
}


uint8_t load_P2(uint8_t cmd,
                    uint8_t scmd,
                    uint8_t len,
                    uint8_t *buf)
{
    (void)cmd;
    (void)scmd;

    if (p2_full_offset == 0) {
        memset(acc_init_full_buffer, 0, sizeof(acc_init_full_buffer));
    }

    memcpy((uint8_t*)acc_init_full_buffer + p2_full_offset, buf, len);
    p2_full_offset += len;

    if (p2_full_offset >= sizeof(acc_init_full_buffer)) {
        p2_full_offset = 0;
    }

    return SS_ERR_OK;
}


uint8_t run_full_p1p1t_times_O(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf)
{
    (void)cmd;
    (void)scmd;
    (void)len;
    (void)buf;

    const int param_v = 78;
    const int param_o = 8;
    const int m_vec_limbs = 5;

    memcpy(acc_full_buffer, acc_init_full_buffer, sizeof(acc_full_buffer));
    acc_full_read_offset = 0;

    const uint64_t *P1 = p1_data;
    const unsigned char *O = O_global;
    uint64_t *acc = acc_full_buffer;

    int bs_mat_entries_used = 0;

    trigger_high();

    for (int r = 0; r < param_v; r++) {
        for (int c = r; c < param_v; c++) {
            if (c == r) {
                bs_mat_entries_used += 1;
                continue;
            }
            for (int k = 0; k < param_o; k++) {
                m_vec_mul_add(
                    m_vec_limbs,
                    P1 + m_vec_limbs * bs_mat_entries_used,
                    O[c * param_o + k],
                    acc + m_vec_limbs * (r * param_o + k)
                );

                m_vec_mul_add(
                    m_vec_limbs,
                    P1 + m_vec_limbs * bs_mat_entries_used,
                    O[r * param_o + k],
                    acc + m_vec_limbs * (c * param_o + k)
                );
            }
            bs_mat_entries_used += 1;
        }
    }

    trigger_low();

    uint8_t ok = 0xA5;
    simpleserial_put('r', 1, &ok);
    return SS_ERR_OK;
}


uint8_t run_one_rc(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf)
{
    (void)cmd;
    (void)scmd;
    (void)len;
    (void)buf;

    const int param_o = 8;
    const int m_vec_limbs = 5;

    memcpy(acc_full_buffer, acc_init_full_buffer, sizeof(acc_full_buffer));

    int r = 0;
    int c = 1;
    int bs_mat_entries_used = 1;

    const uint64_t *P1_entry = p1_data + m_vec_limbs * bs_mat_entries_used;

    trigger_high();

    for (int k = 0; k < param_o; k++) {
        m_vec_mul_add(
            m_vec_limbs,
            P1_entry,
            O_global[c * param_o + k],
            acc_full_buffer + m_vec_limbs * (r * param_o + k)
        );

        m_vec_mul_add(
            m_vec_limbs,
            P1_entry,
            O_global[r * param_o + k],
            acc_full_buffer + m_vec_limbs * (c * param_o + k)
        );
    }

    trigger_low();

    uint8_t ok = 0xA5;
    simpleserial_put('r', 1, &ok);
    return SS_ERR_OK;
}


uint8_t read_full_acc(uint8_t cmd,
                          uint8_t scmd,
                          uint8_t len,
                          uint8_t *buf)
{
    (void)cmd;
    (void)scmd;

    if (len < 1) {
        return SS_ERR_OK;
    }

    uint8_t out_len = buf[0];

    size_t remaining = sizeof(acc_full_buffer) - acc_full_read_offset;
    if (out_len > remaining) {
        out_len = (uint8_t)remaining;
    }

    simpleserial_put('r',
                     out_len,
                     ((uint8_t*)acc_full_buffer) + acc_full_read_offset);

    acc_full_read_offset += out_len;

    if (acc_full_read_offset >= sizeof(acc_full_buffer)) {
        acc_full_read_offset = 0;
    }

    return SS_ERR_OK;
}

uint8_t run_two_muladd(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf)
{
    (void)cmd; (void)scmd; (void)len; (void)buf;

    const int param_o = 8;
    const int m_vec_limbs = 5;

    memcpy(acc_full_buffer, acc_init_full_buffer, sizeof(acc_full_buffer));

    const int r = 0;
    const int c = 1;
    const int bs_mat_entries_used = 1;

    const uint64_t *P1_entry = p1_data + m_vec_limbs * bs_mat_entries_used;
    uint64_t *acc_ptr = acc_full_buffer + m_vec_limbs * (r * param_o + 0);
    unsigned char o_val = O_global[c * param_o + 0];

    trigger_high();
    m_vec_mul_add(m_vec_limbs, P1_entry, o_val, acc_ptr);
    m_vec_mul_add(m_vec_limbs, P1_entry, o_val, acc_ptr);
    trigger_low();

    uint8_t ok = 0xA7;
    simpleserial_put('r', 1, &ok);
    return SS_ERR_OK;
}


int main(void)
{
    platform_init();
    init_uart();
    trigger_setup();

    simpleserial_init();

    simpleserial_addcmd('t', 0, test_cmd);
    simpleserial_addcmd('o', 0, load_O);
    simpleserial_addcmd('i', 0, load_P2);
    simpleserial_addcmd('d', 1, run_one_rc);
    simpleserial_addcmd('g', 1, read_full_acc);
    simpleserial_addcmd('a', 0, run_full_p1p1t_times_O);
    simpleserial_addcmd('y', 0, run_two_muladd);

    while (1) {
        simpleserial_get();
    }
}