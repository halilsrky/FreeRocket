/**
 * @file sensor_mailbox.c
 * @brief Mailbox-based inter-task communication implementation
 * @date 2025-12-26
 * @author halilsrky
 *
 * Implementation of mailbox pattern using FreeRTOS queues with depth=1.
 * Uses xQueueOverwrite() for producers and xQueuePeek() for consumers.
 */

#include "sensor_mailbox.h"
#include "stm32f4xx_hal.h"

/* ============================================================================
 *                           MAILBOX HANDLES
 * ============================================================================ */

osMessageQueueId_t bmiMailboxHandle = NULL;
osMessageQueueId_t bmeMailboxHandle = NULL;
osMessageQueueId_t gnssMailboxHandle = NULL;
osMessageQueueId_t adcMailboxHandle = NULL;
osMessageQueueId_t fusedMailboxHandle = NULL;

/* ============================================================================
 *                        QUEUE ATTRIBUTES
 * ============================================================================ */

static const osMessageQueueAttr_t bmiMailbox_attr = {
    .name = "bmiMailbox"
};

static const osMessageQueueAttr_t bmeMailbox_attr = {
    .name = "bmeMailbox"
};

static const osMessageQueueAttr_t gnssMailbox_attr = {
    .name = "gnssMailbox"
};

static const osMessageQueueAttr_t adcMailbox_attr = {
    .name = "adcMailbox"
};

static const osMessageQueueAttr_t fusedMailbox_attr = {
    .name = "fusedMailbox"
};

/* ============================================================================
 *                           INITIALIZATION
 * ============================================================================ */

void mailbox_init(void)
{
    // Create mailboxes with depth=1 for mailbox pattern
    // Each queue holds exactly one item - the latest sample
    
    bmiMailboxHandle = osMessageQueueNew(1, sizeof(bmi_sample_t), &bmiMailbox_attr);
    bmeMailboxHandle = osMessageQueueNew(1, sizeof(bme_sample_t), &bmeMailbox_attr);
    gnssMailboxHandle = osMessageQueueNew(1, sizeof(gnss_sample_t), &gnssMailbox_attr);
    adcMailboxHandle = osMessageQueueNew(1, sizeof(adc_sample_t), &adcMailbox_attr);
    fusedMailboxHandle = osMessageQueueNew(1, sizeof(fused_sample_t), &fusedMailbox_attr);
    
    // Note: In production, you might want to check if creation succeeded
    // and handle errors appropriately
}

/* ============================================================================
 *                        PRODUCER FUNCTIONS
 * ============================================================================ */

void mailbox_send_bmi(const bmi_sample_t *sample)
{
    if (bmiMailboxHandle != NULL && sample != NULL) {
        xQueueOverwrite(bmiMailboxHandle, sample);
    }
}

void mailbox_send_bme(const bme_sample_t *sample)
{
    if (bmeMailboxHandle != NULL && sample != NULL) {
        xQueueOverwrite(bmeMailboxHandle, sample);
    }
}

void mailbox_send_gnss(const gnss_sample_t *sample)
{
    if (gnssMailboxHandle != NULL && sample != NULL) {
        xQueueOverwrite(gnssMailboxHandle, sample);
    }
}

void mailbox_send_adc(const adc_sample_t *sample)
{
    if (adcMailboxHandle != NULL && sample != NULL) {
        xQueueOverwrite(adcMailboxHandle, sample);
    }
}

void mailbox_send_fused(const fused_sample_t *sample)
{
    if (fusedMailboxHandle != NULL && sample != NULL) {
        xQueueOverwrite(fusedMailboxHandle, sample);
    }
}

/* ============================================================================
 *                        CONSUMER FUNCTIONS
 * ============================================================================ */

BaseType_t mailbox_peek_bmi(bmi_sample_t *sample)
{
    if (bmiMailboxHandle == NULL || sample == NULL) {
        return pdFALSE;
    }
    return xQueuePeek(bmiMailboxHandle, sample, 0);
}

BaseType_t mailbox_peek_bme(bme_sample_t *sample)
{
    if (bmeMailboxHandle == NULL || sample == NULL) {
        return pdFALSE;
    }
    return xQueuePeek(bmeMailboxHandle, sample, 0);
}

BaseType_t mailbox_peek_gnss(gnss_sample_t *sample)
{
    if (gnssMailboxHandle == NULL || sample == NULL) {
        return pdFALSE;
    }
    return xQueuePeek(gnssMailboxHandle, sample, 0);
}

BaseType_t mailbox_peek_adc(adc_sample_t *sample)
{
    if (adcMailboxHandle == NULL || sample == NULL) {
        return pdFALSE;
    }
    return xQueuePeek(adcMailboxHandle, sample, 0);
}

BaseType_t mailbox_peek_fused(fused_sample_t *sample)
{
    if (fusedMailboxHandle == NULL || sample == NULL) {
        return pdFALSE;
    }
    return xQueuePeek(fusedMailboxHandle, sample, 0);
}

/* ============================================================================
 *                        UTILITY FUNCTIONS
 * ============================================================================ */

uint8_t mailbox_is_bmi_valid(void)
{
    if (bmiMailboxHandle == NULL) return 0;
    return (uxQueueMessagesWaiting(bmiMailboxHandle) > 0) ? 1 : 0;
}

uint8_t mailbox_is_bme_valid(void)
{
    if (bmeMailboxHandle == NULL) return 0;
    return (uxQueueMessagesWaiting(bmeMailboxHandle) > 0) ? 1 : 0;
}

uint8_t mailbox_is_gnss_valid(void)
{
    if (gnssMailboxHandle == NULL) return 0;
    return (uxQueueMessagesWaiting(gnssMailboxHandle) > 0) ? 1 : 0;
}

uint8_t mailbox_is_adc_valid(void)
{
    if (adcMailboxHandle == NULL) return 0;
    return (uxQueueMessagesWaiting(adcMailboxHandle) > 0) ? 1 : 0;
}

uint8_t mailbox_is_fused_valid(void)
{
    if (fusedMailboxHandle == NULL) return 0;
    return (uxQueueMessagesWaiting(fusedMailboxHandle) > 0) ? 1 : 0;
}
