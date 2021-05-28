/*
			________________DISCLAIMER!!!!!________________
 THIS PIECE OF SOFTWARE IS JUST A MODIFICATION FROM THE ORIGINAL CODE MADE BY ALEX
		  FROM INSIDEGAGETS SO I DON'T OWN THIS PIECE OF CODE
FOR MORE INFO ABOUT THIS SOFTWARE CHECKOUT THE ORIGINAL WEBPAGE WHERE YOU COULD 
FIND THE HARDWARE USED AND THE ORIGINAL SOFTWARE: https://www.gbxcart.com/
			________________DISCLAIMER!!!!!________________

 GBxCart RW - Console Interface
 Version: 1.35
 Author: Alex from insideGadgets (www.insidegadgets.com)
 Modification Author: Xavi
 Created: 7/11/2016
 Last Modified: 27/05/2021
 License: CC-BY-NC-SA
 
 GBxCart RW allows you to dump your Gameboy/Gameboy Colour/Gameboy Advance games ROM, save the RAM and write to the RAM.
 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "setup.h" // See defines, variables, constants, functions here

char* concat(const char* s1, const char* s2)
{
	char* result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
	// in real code you would check for errors in malloc here
	strcpy(result, s1);
	strcat(result, s2);
	return result;
}

void sigintHandler(int sig_num) {
	set_mode('0');
	RS232_flushRX(cport_nr);
	gbx_cart_power_down();
	exit(0);
}

int main(int argc, char **argv) {
	signal(SIGINT, sigintHandler);
	
	read_config();
	
	// Open COM port
	if (com_test_port() == 0) {
		printf("Device not connected and couldn't be auto detected. Please make sure the COM port isn't open elsewhere. If you are using Linux/Mac, make sure you use \"sudo\".\n");
		read_one_letter();
		return 1;
	}
	printf("Connected on COM port: %i at ", cport_nr+1);
	
	// Break out of any existing functions on ATmega
	set_mode('0');
	RS232_flushRX(cport_nr);
	
	// Get cartridge mode - Gameboy or Gameboy Advance
	cartridgeMode = request_value(CART_MODE);
	
	// Get PCB version
	gbxcartPcbVersion = request_value(READ_PCB_VERSION);
	xmas_wake_up();
	
	// Increase to 1.7Mbit baud for v1.4 PCB
	if (gbxcartPcbVersion == PCB_1_4) {
		set_mode(USART_1_7M_SPEED);
		RS232_CloseComport(cport_nr);
		delay_ms(200);
		
		if (RS232_OpenComport(cport_nr, 1700000, "8N1") == 0) {
			printf("1.7M Baud\n");
		}
	}
	else {
		printf("1M Baud\n");
	}
	
	char pcbVersionString[20];
	if (gbxcartPcbVersion == PCB_1_0) {
		strncpy(pcbVersionString, "v1.0", 5);
	}
	else if (gbxcartPcbVersion == PCB_1_1) {
		strncpy(pcbVersionString, "v1.1/v1.2", 10);
	}
	else if (gbxcartPcbVersion == PCB_1_3) {
		strncpy(pcbVersionString, "v1.3", 5);
	}
	else if (gbxcartPcbVersion == PCB_1_4) {
		strncpy(pcbVersionString, "v1.4", 5);
	}
	else if (gbxcartPcbVersion == MINI) {
		strncpy(pcbVersionString, "Mini", 5);
	}
	else if (gbxcartPcbVersion == GBXMAS) {
		strncpy(pcbVersionString, "XMAS", 5);
	}
	
	// Get firmware version
	gbxcartFirmwareVersion = request_value(READ_FIRMWARE_VERSION);
	printf("PCB version: %s, Firmware version: %i\n", pcbVersionString, gbxcartFirmwareVersion);
	
	
	// Prompt for GB or GBA mode
	if (gbxcartPcbVersion == PCB_1_3 || gbxcartPcbVersion == PCB_1_4 || gbxcartPcbVersion == GBXMAS) {
		printf("\nPlease select a mode:\n"\
				"1. GB/GBC\n"\
				"2. GBA\n"); 
		
		char modeSelected = read_one_letter();
		if (modeSelected == '1') {
			if (gbxcartPcbVersion == GBXMAS) {
				xmas_set_leds(0x6555955);	
			}
			set_mode(VOLTAGE_5V);
		}
		else {
			if (gbxcartPcbVersion == GBXMAS) {
				xmas_set_leds(0x9AAA6AA);
			}
			set_mode(VOLTAGE_3_3V);
		}
	}

	//READING CARTRIDGE HEADER
	printf("\n--- Read Header ---\n");

	if (cartridgeMode == GB_MODE) 
	{
		read_gb_header();
	}
	else 
	{
		read_gba_header();
	}
	//___________________________________

	char titleFilename[30];

	//CARTRIDGE CHECKS BEFORE READING ROM
	if(gameTitle != "" && cartridgeType != 0 && romSize != 0 && ramSize != 0) 
	{
		if (gameTitle[0] != '\0') {
			strncpy(titleFilename, gameTitle, 20);
		}
		else {
			strncpy(titleFilename, "untitled", 9);
		}
		if (cartridgeMode == GB_MODE) {
			strncat(titleFilename, ".gb", 4);
		}
		else {
			strncat(titleFilename, ".gba", 5);
		}

		printf("Reading ROM to %s\n", titleFilename);
		printf("[             25%%             50%%             75%%            100%%]\n[");

		// Create a new file
		FILE* romFile = fopen(titleFilename, "wb");

		uint32_t readBytes = 0;
		if (cartridgeMode == GB_MODE) {
			// Set start and end address
			currAddr = 0x0000;
			endAddr = 0x7FFF;
			xmas_setup((romBanks * 16384) / 28);

			// Read ROM
			uint16_t timedoutCounter = 0;
			for (uint16_t bank = 1; bank < romBanks; bank++) {
				if (cartridgeType >= 5) { // MBC2 and above
					if (bank >= 256) {
						set_bank(0x3000, 1); // High bit
					}
					else {
						set_bank(0x3000, 0); // High bit
					}
					set_bank(0x2100, bank & 0xFF);
				}
				else if (cartridgeType >= 1) { // MBC1
					if ((strncmp(gameTitle, "MOMOCOL", 7) == 0) || (strncmp(gameTitle, "BOMCOL", 6) == 0)) { // MBC1 Hudson
						set_bank(0x4000, bank >> 4);
						if (bank < 10) {
							set_bank(0x2000, bank & 0x1F);
						}
						else {
							set_bank(0x2000, 0x10 | (bank & 0x1F));
						}
					}
					else { // Regular MBC1
						set_bank(0x6000, 0); // Set ROM Mode 
						set_bank(0x4000, bank >> 5); // Set bits 5 & 6 (01100000) of ROM bank
						set_bank(0x2000, bank & 0x1F); // Set bits 0 & 4 (00011111) of ROM bank
					}
				}

				if (bank > 1) { currAddr = 0x4000; }

				// Set start address and rom reading mode
				set_number(currAddr, SET_START_ADDRESS);
				if (fastReadEnabled == 1) {
					set_mode(READ_ROM_4000H);
				}
				else {
					set_mode(READ_ROM_RAM);
				}

				// Read data
				uint8_t localbuffer[257];
				while (currAddr < endAddr) {
					if (fastReadEnabled == 1) {
						uint8_t rxBytes = RS232_PollComport(cport_nr, localbuffer, 64);
						if (rxBytes > 0) {
							localbuffer[rxBytes] = 0;
							fwrite(localbuffer, 1, rxBytes, romFile);
							currAddr += rxBytes;
							readBytes += rxBytes;
							timedoutCounter = 0;
						}
						else {
							timedoutCounter++;
							if (timedoutCounter >= 10000) { // Timed out, restart transfer 1 bank before
								timedoutCounter = 0;
								bank--;
								fseek(romFile, readBytes - (currAddr - 0x4000), SEEK_SET);
								readBytes -= (currAddr - 0x4000);
								break;
							}
						}
						if (bank == 1 && fastReadEnabled == 1 && currAddr == 0x4000) { // Ask for another 32KB (only happens once)
							set_mode(READ_ROM_4000H);
						}
					}
					else {
						uint8_t comReadBytes = com_read_bytes(romFile, 64);
						if (comReadBytes == 64) {
							currAddr += 64;
							readBytes += 64;

							// Request 64 bytes more
							if (currAddr < endAddr) {
								com_read_cont();
							}
						}
						else { // Didn't receive 64 bytes, usually this only happens for Apple MACs
							fflush(romFile);
							com_read_stop();
							delay_ms(500);
							printf("Retrying\n");

							// Flush buffer
							RS232_PollComport(cport_nr, readBuffer, 64);

							// Start off where we left off
							fseek(romFile, readBytes, SEEK_SET);
							set_number(currAddr, SET_START_ADDRESS);
							set_mode(READ_ROM_RAM);
						}
					}

					// Print progress
					print_progress_percent(readBytes, (romBanks * 16384) / 64);
					led_progress_percent(readBytes, (romBanks * 16384) / 28);
				}
				com_read_stop(); // Stop reading ROM (as we will bank switch)
			}
			printf("]");
		}
		else { // GBA mode
			// Set start and end address
			currAddr = 0x00000;
			endAddr = romEndAddr;
			set_number(currAddr, SET_START_ADDRESS);
			xmas_setup(endAddr / 28);
			lastAddrHash = endAddr / 64;

			// Fast reading
			if (fastReadEnabled == 1) {
				uint16_t timedoutCounter = 0;
				set_mode(GBA_READ_ROM_8000H);

				uint8_t buffer[65];
				while (currAddr < endAddr) {
					uint8_t rxBytes = RS232_PollComport(cport_nr, buffer, 64);
					if (rxBytes > 0) {
						buffer[rxBytes] = 0;
						fwrite(buffer, 1, rxBytes, romFile);
						currAddr += rxBytes;
						timedoutCounter = 0;
					}
					else {
						timedoutCounter++;
						if (timedoutCounter >= 10000) {
							timedoutCounter = 0;
							RS232_PollComport(cport_nr, readBuffer, 256); // Flush

							if (currAddr >= 0x20000) {
								uint32_t hexCalc = ((currAddr / 0x10000) - 1);
								uint32_t calculateRewind = 0x10000 * hexCalc;
								fseek(romFile, calculateRewind, SEEK_SET);
								currAddr = calculateRewind;
								set_number(currAddr / 2, SET_START_ADDRESS);
							}
							else {
								fseek(romFile, 0, SEEK_SET);
								currAddr = 0;
								set_mode('0');
								delay_ms(5);
								set_number(currAddr, SET_START_ADDRESS);
								delay_ms(5);
							}
						}
					}

					if (currAddr % 0x10000 == 0 && currAddr != endAddr) {
						set_mode(GBA_READ_ROM_8000H);
					}

					// Print progress
					print_progress_percent(currAddr, endAddr / 64);
					led_progress_percent(currAddr, endAddr / 28);
				}
				printf("]");
				com_read_stop();
			}
			else {
				uint16_t readLength = 64;
				set_mode(GBA_READ_ROM);

				// Read data
				while (currAddr < endAddr) {
					uint8_t comReadBytes = com_read_bytes(romFile, readLength);
					if (comReadBytes == readLength) {
						currAddr += readLength;

						// Request 64 bytes more
						if (currAddr < endAddr) {
							com_read_cont();
						}
					}
					else { // Didn't receive 64 bytes
						fflush(romFile);
						com_read_stop();
						delay_ms(500);

						// Flush buffer
						RS232_PollComport(cport_nr, readBuffer, readLength);

						// Start off where we left off
						fseek(romFile, currAddr, SEEK_SET);
						set_number(currAddr / 2, SET_START_ADDRESS);
						set_mode(GBA_READ_ROM);
					}

					// Print progress
					print_progress_percent(currAddr, endAddr / 64);
					led_progress_percent(currAddr, endAddr / 28);
				}
				printf("]");
				com_read_stop();
			}
		}

		fclose(romFile);
		gbx_set_done_led();
		printf("\nFinished\n");

		//GETTING THE SAVE
		if (cartridgeMode == GB_MODE) {
			// Does cartridge have RAM
			if (ramEndAddress > 0 && headerCheckSumOk == 1) {
				char titleFilename[30];
				if (gameTitle[0] != '\0') {
					strncpy(titleFilename, gameTitle, 20);
				}
				else {
					strncpy(titleFilename, "untitled", 9);
				}
				strncat(titleFilename, ".sav", 4);

				// Check if file exists
				FILE* ramFile = fopen(titleFilename, "rb");
				char confirmWrite = 'y';
				if (ramFile != NULL) {
					printf("File %s exists on your PC.", titleFilename);
					printf("\n\n*** This will erase the save game from your PC ***");
					printf("\nPress y to continue or any other key to abort.\n");

					//confirmWrite = read_one_letter();
					printf("\n");
					fclose(ramFile);
				}

				Sleep(2000);

				if (true) {
					printf("Backing up save to %s\n", titleFilename);
					printf("[             25%%             50%%             75%%            100%%]\n[");

					// Create a new file
					FILE* ramFile = fopen(titleFilename, "wb");

					mbc2_fix();
					if (cartridgeType <= 4) { // MBC1
						set_bank(0x6000, 1); // Set RAM Mode
					}
					set_bank(0x0000, 0x0A); // Initialise MBC

					// Check if Gameboy Camera cart with v1.0/1.1 PCB with R1 firmware, read data slower
					if (cartridgeType == 252 && gbxcartFirmwareVersion == 1) {
						// Read RAM
						uint32_t readBytes = 0;
						for (uint8_t bank = 0; bank < ramBanks; bank++) {
							uint16_t ramAddress = 0xA000;
							set_bank(0x4000, bank);
							set_number(ramAddress, SET_START_ADDRESS); // Set start address again

							RS232_cputs(cport_nr, "M0"); // Disable CS/RD/WR/CS2-RST from going high after each command
							RS232_drain(cport_nr);
							delay_ms(5);

							set_mode(GB_CART_MODE);

							while (ramAddress < ramEndAddress) {
								for (uint8_t x = 0; x < 64; x++) {

									char hexNum[7];
									sprintf(hexNum, "HA0x%x", ((ramAddress + x) >> 8));
									RS232_cputs(cport_nr, hexNum);
									RS232_SendByte(cport_nr, 0);
									RS232_drain(cport_nr);

									sprintf(hexNum, "HB0x%x", ((ramAddress + x) & 0xFF));
									RS232_cputs(cport_nr, hexNum);
									RS232_SendByte(cport_nr, 0);
									RS232_drain(cport_nr);

									RS232_cputs(cport_nr, "LD0x60"); // cs_mreqPin_low + rdPin_low
									RS232_SendByte(cport_nr, 0);
									RS232_drain(cport_nr);

									RS232_cputs(cport_nr, "DC");
									RS232_drain(cport_nr);

									RS232_cputs(cport_nr, "HD0x60"); // cs_mreqPin_high + rdPin_high
									RS232_SendByte(cport_nr, 0);
									RS232_drain(cport_nr);

									RS232_cputs(cport_nr, "LA0xFF");
									RS232_SendByte(cport_nr, 0);
									RS232_drain(cport_nr);

									RS232_cputs(cport_nr, "LB0xFF");
									RS232_SendByte(cport_nr, 0);
									RS232_drain(cport_nr);
								}

								com_read_bytes(ramFile, 64);

								ramAddress += 64;
								readBytes += 64;

								// Request 64 bytes more
								if (ramAddress < ramEndAddress) {
									com_read_cont();
								}

								// Print progress
								if (ramEndAddress == 0xA1FF) {
									print_progress_percent(readBytes, 64);
								}
								else if (ramEndAddress == 0xA7FF) {
									print_progress_percent(readBytes / 4, 64);
								}
								else {
									print_progress_percent(readBytes, (ramBanks * (ramEndAddress - 0xA000 + 1)) / 64);
								}
							}
							com_read_stop(); // Stop reading RAM (as we will bank switch)
						}

						RS232_cputs(cport_nr, "M1");
						RS232_drain(cport_nr);
					}

					else {
						if (ramEndAddress == 0xA1FF) {
							xmas_setup(ramEndAddress / 28);
						}
						else if (ramEndAddress == 0xA7FF) {
							xmas_setup(ramEndAddress / 4 / 28);
						}
						else {
							xmas_setup((ramBanks * (ramEndAddress - 0xA000 + 1)) / 28);
						}

						// Read RAM
						uint32_t readBytes = 0;
						for (uint8_t bank = 0; bank < ramBanks; bank++) {
							uint16_t ramAddress = 0xA000;
							set_bank(0x4000, bank);
							set_number(ramAddress, SET_START_ADDRESS); // Set start address again
							set_mode(READ_ROM_RAM); // Set rom/ram reading mode

							while (ramAddress < ramEndAddress) {
								uint8_t comReadBytes = com_read_bytes(ramFile, 64);
								if (comReadBytes == 64) {
									ramAddress += 64;
									readBytes += 64;

									// Request 64 bytes more
									if (ramAddress < ramEndAddress) {
										com_read_cont();
									}
								}
								else { // Didn't receive 64 bytes, usually this only happens for Apple MACs
									fflush(ramFile);
									com_read_stop();
									delay_ms(500);
									printf("Retrying\n");

									// Flush buffer
									RS232_PollComport(cport_nr, readBuffer, 64);

									// Start off where we left off
									fseek(ramFile, readBytes, SEEK_SET);
									set_number(ramAddress, SET_START_ADDRESS);
									set_mode(READ_ROM_RAM);
								}

								// Print progress
								if (ramEndAddress == 0xA1FF) {
									print_progress_percent(readBytes, 64);
									led_progress_percent(readBytes, 28);
								}
								else if (ramEndAddress == 0xA7FF) {
									print_progress_percent(readBytes / 4, 64);
									led_progress_percent(readBytes / 4, 28);
								}
								else {
									print_progress_percent(readBytes, (ramBanks * (ramEndAddress - 0xA000 + 1)) / 64);
									led_progress_percent(readBytes, (ramBanks * (ramEndAddress - 0xA000 + 1)) / 28);
								}
							}
							com_read_stop(); // Stop reading RAM (as we will bank switch)
						}
					}
					printf("]");

					set_bank(0x4000, 0x00); // Stop rumble if it's present
					set_bank(0x0000, 0x00); // Disable RAM

					fclose(ramFile);
					gbx_set_done_led();
					printf("\nFinished\n");
				}
				else {
					printf("Aborted\n");
					gbx_set_error_led();
				}
			}
			else {
				printf("Cartridge has no RAM\n");
			}
		}
		else { // GBA mode
			// Does cartridge have RAM
			if (ramEndAddress > 0 || eepromEndAddress > 0) {
				char titleFilename[30];
				if (gameTitle[0] != '\0') {
					strncpy(titleFilename, gameTitle, 20);
				}
				else {
					strncpy(titleFilename, "untitled", 9);
				}
				strncat(titleFilename, ".sav", 4);

				// Check if file exists
				FILE* ramFile = fopen(titleFilename, "rb");
				char confirmWrite = 'y';
				if (ramFile != NULL) {
					printf("File %s exists on your PC.", titleFilename);
					printf("\n\n*** This will erase the save game from your PC ***");
					printf("\nPress y to continue or any other key to abort.\n");

					//confirmWrite = read_one_letter();
					printf("\n");
					fclose(ramFile);
				}

				if (true) {
					// Create a new file
					FILE* ramFile = fopen(titleFilename, "wb");

					// SRAM/Flash
					if (ramEndAddress > 0) {
						printf("Backing up save (SRAM/Flash) to %s\n", titleFilename);
						printf("[             25%%             50%%             75%%            100%%]\n[");

						xmas_setup((ramBanks * ramEndAddress) / 28);

						// Read RAM
						uint32_t readBytes = 0;
						for (uint8_t bank = 0; bank < ramBanks; bank++) {
							// Flash, switch bank 1
							if (hasFlashSave >= FLASH_FOUND && bank == 1) {
								set_number(1, GBA_FLASH_SET_BANK);
							}
							else if (hasFlashSave == NO_FLASH && bank == 1) { // 1Mbit SRAM
								gba_flash_write_address_byte(0x1000000, 0x1);
							}

							// Set start and end address
							currAddr = 0x00000;
							endAddr = ramEndAddress;
							set_number(currAddr, SET_START_ADDRESS);
							set_mode(GBA_READ_SRAM);

							while (currAddr < endAddr) {
								uint8_t comReadBytes = com_read_bytes(ramFile, 64);
								if (comReadBytes == 64) {
									currAddr += 64;
									readBytes += 64;

									// Request 64 bytes more
									if (currAddr < endAddr) {
										com_read_cont();
									}
								}
								else { // Didn't receive 64 bytes, usually this only happens for Apple MACs
									fflush(ramFile);
									com_read_stop();
									delay_ms(500);
									printf("Retrying\n");

									// Flush buffer
									RS232_PollComport(cport_nr, readBuffer, 64);

									// Start off where we left off
									fseek(ramFile, currAddr, SEEK_SET);
									set_number(currAddr, SET_START_ADDRESS);
									set_mode(GBA_READ_SRAM);
								}

								print_progress_percent(readBytes, (ramBanks * ramEndAddress) / 64);
								led_progress_percent(readBytes, (ramBanks * ramEndAddress) / 28);
							}

							com_read_stop(); // End read (for bank if flash)

							// Flash, switch back to bank 0
							if (hasFlashSave >= FLASH_FOUND && bank == 1) {
								set_number(0, GBA_FLASH_SET_BANK);
							}
							// SRAM 1Mbit, switch back to bank 0
							else if (hasFlashSave == NO_FLASH && bank == 1) {
								gba_flash_write_address_byte(0x1000000, 0x0);
							}
						}
					}

					// EEPROM
					else {
						printf("Backing up save (EEPROM) to %s\n", titleFilename);
						printf("[             25%%             50%%             75%%            100%%]\n[");

						xmas_setup(eepromEndAddress / 28);
						set_number(eepromSize, GBA_SET_EEPROM_SIZE);

						// Set start and end address
						currAddr = 0x000;
						endAddr = eepromEndAddress;
						set_number(currAddr, SET_START_ADDRESS);
						set_mode(GBA_READ_EEPROM);

						// Read EEPROM
						uint32_t readBytes = 0;
						while (currAddr < endAddr) {
							com_read_bytes(ramFile, 8);
							currAddr += 8;
							readBytes += 8;

							// Request 8 bytes more
							if (currAddr < endAddr) {
								com_read_cont();
							}

							print_progress_percent(readBytes, endAddr / 64);
							led_progress_percent(readBytes, endAddr / 28);
						}

						com_read_stop(); // End read
					}

					fclose(ramFile);
					gbx_set_done_led();
					printf("]");
					printf("\nFinished\n");
				}
				else {
					printf("Aborted\n");
					gbx_set_error_led();
				}
			}
			else {
				printf("Cartridge has no RAM\n");
			}
		}
		//________________
	}
	//ERROR!!!!!
	else 
	{
		exit(0);
	}

	//OPENING THE EMULATOR WITH THE ROM
	printf(concat("VisualBoyAdvance.exe ", titleFilename));
	int status = system(concat("VisualBoyAdvance.exe ", titleFilename));
	//___________________________________

	//UPLOADING THE SAVE TO THE CARTRIDGE
	printf("\n--- Restore save from PC to Cartridge ---\n");

	if (cartridgeMode == GB_MODE) {
		// Does cartridge have RAM
		if (ramEndAddress > 0 && headerCheckSumOk == 1) {
			char titleFilename[30];
			if (gameTitle[0] != '\0') {
				strncpy(titleFilename, gameTitle, 20);
			}
			else {
				strncpy(titleFilename, "untitled", 9);
			}
			strncat(titleFilename, ".sav", 4);

			// Open file
			FILE* ramFile = fopen(titleFilename, "rb");
			if (ramFile != NULL) {
				printf("Going to write save from %s...", titleFilename);
				printf("\n\n*** This will erase the save game from your Gameboy Cartridge ***");
				printf("\nPress y to continue or any other key to abort.\n");

				//char confirmWrite = read_one_letter();
				if (true) {
					printf("\nRestoring save from %s\n", titleFilename);
					printf("[             25%%             50%%             75%%            100%%]\n[");

					mbc2_fix();
					if (cartridgeType <= 4) { // MBC1
						set_bank(0x6000, 1); // Set RAM Mode
					}
					set_bank(0x0000, 0x0A); // Initialise MBC

					if (ramEndAddress == 0xA1FF) {
						xmas_setup(ramEndAddress / 28);
					}
					else if (ramEndAddress == 0xA7FF) {
						xmas_setup(ramEndAddress / 4 / 28);
					}
					else {
						xmas_setup((ramBanks * (ramEndAddress - 0xA000 + 1)) / 28);
					}

					// Write RAM
					uint32_t readBytes = 0;
					for (uint8_t bank = 0; bank < ramBanks; bank++) {
						uint16_t ramAddress = 0xA000;
						set_bank(0x4000, bank);
						set_number(0xA000, SET_START_ADDRESS); // Set start address again

						while (ramAddress < ramEndAddress) {
							com_write_bytes_from_file(WRITE_RAM, ramFile, 64);
							ramAddress += 64;
							readBytes += 64;
							com_wait_for_ack();

							// Print progress
							if (ramEndAddress == 0xA1FF) {
								print_progress_percent(readBytes, 64);
								led_progress_percent(readBytes, 28);
							}
							else if (ramEndAddress == 0xA7FF) {
								print_progress_percent(readBytes / 4, 64);
								led_progress_percent(readBytes / 4, 28);
							}
							else {
								print_progress_percent(readBytes, (ramBanks * (ramEndAddress - 0xA000 + 1)) / 64);
								led_progress_percent(readBytes, (ramBanks * (ramEndAddress - 0xA000 + 1)) / 28);
							}
						}
					}
					printf("]");
					set_bank(0x4000, 0x00); // Stop rumble if it's present
					set_bank(0x0000, 0x00); // Disable RAM

					fclose(ramFile);
					gbx_set_done_led();
					printf("\nFinished\n");
				}
				else {
					printf("Aborted\n");
					gbx_set_error_led();
				}
			}
			else {
				printf("%s File not found\n", titleFilename);
			}
		}
		else {
			printf("Cartridge has no RAM\n");
		}
	}
	else { // GBA mode
		// Does cartridge have RAM
		if (ramEndAddress > 0 || eepromEndAddress > 0) {
			char titleFilename[30];
			if (gameTitle[0] != '\0') {
				strncpy(titleFilename, gameTitle, 20);
			}
			else {
				strncpy(titleFilename, "untitled", 9);
			}
			strncat(titleFilename, ".sav", 4);

			// Open file
			FILE* ramFile = fopen(titleFilename, "rb");
			if (ramFile != NULL) {
				// SRAM/Flash or EEPROM
				if (eepromSize == EEPROM_NONE) {
					// Check if it's SRAM or Flash (if we haven't checked before)
					if (hasFlashSave == NOT_CHECKED) {
						hasFlashSave = gba_test_sram_flash_write();
					}

					if (hasFlashSave >= FLASH_FOUND) {
						printf("Going to write save to Flash from %s", titleFilename);
					}
					else {
						printf("Going to write save to SRAM from %s", titleFilename);
					}
				}
				else {
					printf("Going to write save to EEPROM from %s", titleFilename);
				}

				printf("\n\n*** This will erase the save game from your Gameboy Advance Cartridge ***");
				printf("\nPress y to continue or any other key to abort.\n");

				//char confirmWrite = read_one_letter();
				if (true) {
					if (eepromSize == EEPROM_NONE) {
						if (hasFlashSave >= FLASH_FOUND) {
							printf("\nWriting Save to Flash from %s", titleFilename);
						}
						else {
							printf("\nWriting Save to SRAM from %s", titleFilename);
						}
					}
					else {
						printf("\nWriting Save to EEPROM from %s", titleFilename);
					}
					printf("\n[             25%%             50%%             75%%            100%%]\n[");

					// SRAM
					if (hasFlashSave == NO_FLASH && eepromSize == EEPROM_NONE) {
						xmas_setup((ramEndAddress * ramBanks) / 28);

						uint32_t readBytes = 0;
						for (uint8_t bank = 0; bank < ramBanks; bank++) {
							if (bank == 1) { // 1Mbit SRAM
								gba_flash_write_address_byte(0x1000000, 0x1);
							}

							// Set start and end address
							currAddr = 0x0000;
							endAddr = ramEndAddress;
							set_number(currAddr, SET_START_ADDRESS);

							// Write
							while (currAddr < endAddr) {
								com_write_bytes_from_file(GBA_WRITE_SRAM, ramFile, 64);
								currAddr += 64;
								readBytes += 64;
								com_wait_for_ack();

								print_progress_percent(readBytes, ramEndAddress * ramBanks / 64);
								led_progress_percent(readBytes, ramEndAddress * ramBanks / 28);
							}

							// SRAM 1Mbit, switch back to bank 0
							if (bank == 1) {
								gba_flash_write_address_byte(0x1000000, 0x0);
							}
						}
					}

					// EEPROM
					else if (eepromSize != EEPROM_NONE) {
						xmas_setup(eepromEndAddress / 28);
						set_number(eepromSize, GBA_SET_EEPROM_SIZE);

						// Set start and end address
						currAddr = 0x000;
						endAddr = eepromEndAddress;
						set_number(currAddr, SET_START_ADDRESS);

						// Write
						uint32_t readBytes = 0;
						while (currAddr < endAddr) {
							com_write_bytes_from_file(GBA_WRITE_EEPROM, ramFile, 8);
							currAddr += 8;
							readBytes += 8;

							// Wait for ATmega to process write (~320us) and for EEPROM to write data (6ms)
							com_wait_for_ack();

							print_progress_percent(readBytes, endAddr / 64);
							led_progress_percent(readBytes, endAddr / 28);
						}
					}

					// Flash
					else if (hasFlashSave != NO_FLASH) {
						xmas_setup((ramBanks * endAddr) / 28);

						uint32_t readBytes = 0;
						for (uint8_t bank = 0; bank < ramBanks; bank++) {
							// Set start and end address
							currAddr = 0x0000;
							endAddr = ramEndAddress;
							set_number(currAddr, SET_START_ADDRESS);

							// Program flash in 128 bytes at a time
							if (hasFlashSave == FLASH_FOUND_ATMEL) {
								while (currAddr < endAddr) {
									com_write_bytes_from_file(GBA_FLASH_WRITE_ATMEL, ramFile, 128);
									currAddr += 128;
									readBytes += 128;
									com_wait_for_ack(); // Wait for write complete

									print_progress_percent(readBytes, (ramBanks * endAddr) / 64);
									led_progress_percent(readBytes, (ramBanks * endAddr) / 28);
								}
							}
							else { // Program flash in 1 byte at a time
								if (bank == 1) {
									set_number(1, GBA_FLASH_SET_BANK); // Set bank 1
								}

								uint8_t sector = 0;
								while (currAddr < endAddr) {
									if (currAddr % 4096 == 0) {
										flash_4k_sector_erase(sector);
										sector++;
										com_wait_for_ack(); // Wait 25ms for sector erase

										// Wait for first byte to be 0xFF, that's when we know the sector has been erased
										readBuffer[0] = 0;
										while (readBuffer[0] != 0xFF) {
											set_number(currAddr, SET_START_ADDRESS);
											set_mode(GBA_READ_SRAM);

											com_read_bytes(READ_BUFFER, 64);
											com_read_stop();

											if (readBuffer[0] != 0xFF) {
												delay_ms(5);
											}
										}

										// Set start address again
										set_number(currAddr, SET_START_ADDRESS);

										delay_ms(5); // Wait a little bit as hardware might not be ready
									}

									com_write_bytes_from_file(GBA_FLASH_WRITE_BYTE, ramFile, 64);
									currAddr += 64;
									readBytes += 64;
									com_wait_for_ack(); // Wait for write complete

									print_progress_percent(readBytes, (ramBanks * endAddr) / 64);
									led_progress_percent(readBytes, (ramBanks * endAddr) / 28);
								}
							}

							if (bank == 1) {
								set_number(0, GBA_FLASH_SET_BANK); // Set bank 0 again
							}
						}
					}
					printf("]");

					fclose(ramFile);
					gbx_set_done_led();
					printf("\nFinished\n");
				}
				else {
					printf("Aborted\n");
					gbx_set_error_led();
				}
			}
			else {
				printf("%s File not found\n", titleFilename);
			}
		}
		else {
			printf("Cartridge has no RAM\n");
		}
	}
	
	gbx_cart_power_down();
	
	return 0;
}