#ifndef __LOADER_H__
#define __LOADER_H__


typedef struct loader_s
{
	char *rom;
	char *base;
	char *sram;
	char *state;
	int ramloaded;
} loader_t;


extern loader_t loader;

void loader_init(char *s);
void loader_init_raw(uint8_t *romdata, size_t rom_data_size);
void loader_unload();
int rom_load();
int rom_load_raw(uint8_t *romdata, size_t rom_data_size);
int sram_load();
int sram_save();
void state_load(int n);
void state_save(int n);



#endif
