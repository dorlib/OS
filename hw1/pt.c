#include "os.h"
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn){
    const uint64_t VALID_BIT_MASK = 1;
    uint64_t vpn_indices[5];
    uint64_t* page_table_pointers[5];
    pt = pt << 12; // now pt holds the page table's address.

    for (int i = 0; i < 5; i++) {
        vpn_indices[i] = (vpn >> (36 - (9 * i))) & 0x1FF;
    }

    page_table_pointers[0]  = (uint64_t*)(phys_to_virt(pt));
    if (page_table_pointers[0] == NULL) {
        fprintf(stderr, "Error! Failed to convert physical address to virtual address.\n");
        return;
    }

    if (ppn == NO_MAPPING) {
        for (int i = 0; i < 4; i++) {
            uint64_t current_entry = page_table_pointers[i][vpn_indices[i]];
            uint64_t physical_address = current_entry & ~1;
            void* virtual_address = phys_to_virt(physical_address);
            page_table_pointers[i + 1] = (uint64_t*)virtual_address;

            if (page_table_pointers[i + 1] == NULL || (current_entry & VALID_BIT_MASK) == 0) {
                fprintf(stderr, "Invalid or NULL page table entry at level %d.\n", i);
                return;
            }
        }

        page_table_pointers[4][vpn_indices[4]] = NO_MAPPING;
    } else {
        for (int i = 0; i < 4; i++) {
            if (!page_table_pointers[i]) {
                fprintf(stderr, "NULL pointer encountered at level %d.\n", i);
                return;
            }

            uint64_t current_entry = page_table_pointers[i][vpn_indices[i]];

            if (current_entry == NO_MAPPING || (current_entry & VALID_BIT_MASK) == 0) {
                uint64_t new_frame = alloc_page_frame();

                if (new_frame == 0) {
                    fprintf(stderr, "Failed to allocate new page frame at level %d.\n", i);
                    return;
                }

                uint64_t aligned_frame = new_frame << 12;
                uint64_t entry_with_valid_bit = aligned_frame | 1;
                page_table_pointers[i][vpn_indices[i]] = entry_with_valid_bit;
                current_entry = entry_with_valid_bit; // Update current_entry for next iteration
            }

            uint64_t physical_address = current_entry & ~1;
            void* virtual_address = phys_to_virt(physical_address);

            if (virtual_address == NULL) {
                fprintf(stderr, "Failed to convert physical address to virtual address at level %d.\n", i);
                return;
            }

            page_table_pointers[i + 1] = (uint64_t*)virtual_address;
        }

        page_table_pointers[4][vpn_indices[4]] = (ppn << 12) | 1;
    }
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn){
    const uint64_t VALID_BIT_MASK = 1;
    uint64_t* page_table_pointers[5];
    uint64_t vpn_indices[5];
    pt = pt<<12;

    for (int i = 0; i < 5; i++) {
        vpn_indices[i] = (vpn >> (36 - (9 * i))) & 0x1FF;
    }

    page_table_pointers[0]  = (uint64_t*)(phys_to_virt(pt));
    for (int i = 0; i < 4; i++) {
        uint64_t current_entry = page_table_pointers[i][vpn_indices[i]];
        if (((current_entry) & 1) == 0 || current_entry == NO_MAPPING || page_table_pointers[i] == NULL) {
            return NO_MAPPING;
        }

        page_table_pointers[i + 1] = (uint64_t*)(phys_to_virt(page_table_pointers[i][vpn_indices[i]] - 1));
    }

    uint64_t valid_bit = page_table_pointers[4][vpn_indices[4]];
    if ((valid_bit & VALID_BIT_MASK) == 0 || valid_bit == NO_MAPPING) {
        return NO_MAPPING;
    }

    return valid_bit >> 12;
}
