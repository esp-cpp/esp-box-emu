#include "InterruptManager.hpp"
#include "Gui.hpp"
#include "InesParser.hpp"
#include "Mapper.hpp"
#include "Ppu.hpp"
#include "spi_lcd.h"

Ppu::Ppu(InesParser* ines_parser, Gui* gui, Mapper* mapper){
    this->gui = gui;
    this->ines_parser = ines_parser;
    assert(this->ines_parser!=NULL);
    assert(this->gui!=NULL);
    memcpy(this->vram.pattern_table.raw, this->ines_parser->GetChrRom(), this->ines_parser->GetChrSize());
    this->registers[PPUSTATUS_KIND] = 0x00;
    this->now_cycle = 0;
    this->line = 0;
    this->scroll_x = 0;
    this->scroll_y = 0;
    this->scroll_x_flg = true;
    if(this->ines_parser->IsHorizontal()){
        this->horizontal_mirror = true;
        this->vertical_mirror   = false;
    }else{
        this->horizontal_mirror = false;
        this->vertical_mirror   = true;
    }
    this->lower_byte = false;
    this->ResetTransparentBuff();
    this->mapper = mapper;
    assert(this->mapper!=NULL);
}

uint8_t Ppu::Read(PPU_REGISTER_KIND ppu_register_kind){
    static uint32_t cnt = 0;
    uint8_t data;
    switch (ppu_register_kind){
        case PPUMASK_KIND:
            return this->registers[PPUMASK_KIND];
        case PPUSTATUS_KIND:
            return this->ReadStatus();
        case PPUDATA_KIND:
            data = this->vram_data_buff;
            if(this->vram_addr>=0&&this->vram_addr<=0x1FFF){
                this->vram_data_buff = *(this->mapper->ReadChrRom(this->vram_addr));
            }else{
                this->vram_data_buff = this->vram.raw[this->vram_addr];
            }
            if(this->ppu_ctrl_register.flgs.ppu_addr_inc){
                this->vram_addr+=32;
            }else{
                this->vram_addr++;
            }
            return data;
        case OAMDATA_KIND:
            return this->registers[OAMDATA_KIND];
        default:
            this->Error("Not implemented: %d at Ppu::Read\n", ppu_register_kind);
            return 0;
    }
}

void Ppu::Write(PPU_REGISTER_KIND ppu_register_kind, uint8_t value){
    static uint32_t cnt = 0;
    switch (ppu_register_kind){
        case PPUCTRL_KIND:
            this->ppu_ctrl_register.raw = value;
            if(this->ppu_ctrl_register.flgs.name_num==0x00){
                this->name_table_addr = 0x2000;
                this->name_table_id = this->ppu_ctrl_register.flgs.name_num;
            }else if(this->ppu_ctrl_register.flgs.name_num==0x01){
                this->name_table_addr = 0x2400;
                this->name_table_id = this->ppu_ctrl_register.flgs.name_num;
            }else if(this->ppu_ctrl_register.flgs.name_num==0x02){
                this->name_table_addr = 0x2800;
                this->name_table_id = this->ppu_ctrl_register.flgs.name_num;
            }else if(this->ppu_ctrl_register.flgs.name_num==0x03){
                this->name_table_addr = 0x2C00;
                this->name_table_id = this->ppu_ctrl_register.flgs.name_num;
            }
            return;
        case PPUMASK_KIND:
            this->registers[PPUMASK_KIND] = value;
            if((value&10)!=0){
                this->sprite_enable = true;
            }
            if((value&0x40)!=0){
                this->bg_enable = true;
            }
            return;
        case PPUSCROLL_KIND:
            this->registers[PPUSCROLL_KIND] = value;
            this->WriteScroll();
            return;
        case OAMADDR_KIND:
            this->registers[OAMADDR_KIND] = value;
            return;
        case OAMDATA_KIND:
            this->sprite_ram.raw[this->registers[OAMADDR_KIND]] = value;
            this->registers[OAMADDR_KIND]++;
            return;
        case PPUADDR_KIND:
            if(!this->lower_byte){
                this->vram_addr =  ((uint16_t)value)<<8;
                this->lower_byte = true;
            }else{
                this->vram_addr =  this->vram_addr | value;
                this->lower_byte = false;
            }
            return;
        case PPUDATA_KIND:
            if(this->vram_addr>=0x3F20&&this->vram_addr<=0x3FFF){
                this->vram_addr = 0x3F00+((this->vram_addr-0x3F20)%0x20);
            }
            if(this->vram_addr>=0x3000&&this->vram_addr<=0x3EFF){
                this->Error("Not implemented: ppuio(0x3000~0x3FFF) at Ppu::Write");
            }
            if(this->IsPaletteMirror()){
                this->vram.raw[this->vram_addr-0x10] = value;
            }else{
                this->vram.raw[this->vram_addr] = value;
            }
            if(this->ppu_ctrl_register.flgs.ppu_addr_inc){
                this->vram_addr += 32;
            }else{
                this->vram_addr++;
            }
            return;
        case PPUSTATUS_KIND:
            this->registers[PPUSTATUS_KIND] = value;
            return;
        default:
            this->Error("Not implemented: %d at Ppu::Write\n", ppu_register_kind);
    }
}

bool Ppu::IsPaletteMirror(){
    if(this->vram_addr==0x3F10 || this->vram_addr==0x3F14 || this->vram_addr==0x3F18 || this->vram_addr==0x3F1C){
        return true;
    }
    return false;
}

uint8_t Ppu::GetChrIdx(int x, int y){
    uint32_t relative_x = (x+this->GetScrollX())%WIDTH;
    uint32_t relative_y = (y+this->GetScrollY())%HEIGHT;
    uint32_t addr;
    relative_x = relative_x / 8;
    relative_y = relative_y / 8;
    uint32_t name_table_id;
    name_table_id = ((x+this->GetScrollX())/WIDTH)%2 + (((y+this->GetScrollY())/HEIGHT)%2 ? 2 : 0);
    uint32_t offset;
    offset = relative_x+relative_y*32;
    addr = 0x2000+0x400*name_table_id+offset;
    if(this->horizontal_mirror){
        if(addr>=0x2000&&addr<=0x27FF){
            return this->vram.raw[0x2000+offset];
        }else{
            return this->vram.raw[0x2800+offset];
        }
    }else{
        if((addr>=0x2000&&addr<=0x23FF) || (addr>=0x2800&&addr<=0x2BFF)){
            return this->vram.raw[0x2000+offset];
        }else{
            return this->vram.raw[0x2400+offset];
        }
    }
}

uint8_t* Ppu::GetBg(int idx){
    unsigned int offset = this->ppu_ctrl_register.flgs.bg_addr? 0x1000: 0x0000;
    return this->mapper->ReadChrRom(offset+16*idx);
}

uint8_t* Ppu::GetSprite(int idx){
    unsigned int offset = this->ppu_ctrl_register.flgs.sprite_addr? 0x1000: 0x0000;
    return this->mapper->ReadChrRom(offset+16*idx);
}

Sprite* Ppu::GetSprite(int x, int y){
    for(int i=0; i<64; i++){
        if(this->sprite_ram.sprites[i].x==x&&(this->sprite_ram.sprites[i].y+1)==y){
            return  &this->sprite_ram.sprites[i];
        }
    }
    return NULL;
}

uint32_t Ppu::GetSpritePalette(int palette_id){
    uint8_t p[sizeof(uint32_t)];
    int idx;
    for(int i=0; i<sizeof(uint32_t); i++){
        idx = 0x3F10+palette_id+i;
        if(!(idx%4)){
            *(p+i) = this->vram.raw[0x3F00];
            continue;
        }
        *(p+i) = this->vram.raw[idx];
    }
    return *((uint32_t*)p);
}

bool Ppu::Execute(int cycle, InterruptManager* interrupt_manager){
    this->now_cycle += cycle;
    if(this->now_cycle>=341){
        this->now_cycle -= 341;
        this->line++;
        //this->sprite_list.clear();
        //this->SearchSprite();
        if(this->IsSprite0()){
            this->SetSprite0();
        }
        if(this->line<=240&&this->scroll_y<=240){
            this->DrawBg();
        }   
        if(this->line==241){
            this->SetVblank();
            if(this->ppu_ctrl_register.flgs.nmi){
                interrupt_manager->SetNmi();
            }
        }

        if(this->line==262){
            this->DrawSprites();
            this->ResetTransparentBuff();
            this->ClearSprite0();
            this->ClearVblank();
            interrupt_manager->ClearNmi();
            this->line = 0;
            return true;
        }
    }
    return false;
}

void Ppu::FlipSpriteHorizontally(){
    uint8_t temp;
    for(int i=0; i<8; i++){
        for(int j=0; j<4; j++){
            temp           = this->sprite[i][7-j];
            this->sprite[i][7-j] = this->sprite[i][j];
            this->sprite[i][j]   = temp;
        }
    }
}

void Ppu::FlipSpriteVertically(){
    uint8_t temp;
    for(int i=0; i<8; i++){
        for(int j=0; j<4; j++){
            temp           = this->sprite[7-j][i];
            this->sprite[7-j][i] = this->sprite[j][i];
            this->sprite[j][i]   = temp;
        }
    }
}

void Ppu::DrawSprites(){
    uint32_t palette_idx;
    for(int idx=0; idx<64; idx++){
        Sprite* sprite_info = &this->sprite_ram.sprites[idx];
        this->chr = this->GetSprite(sprite_info->tile_id);
        for(int i=0; i<8; i++){
            int k = 7;
            for(int j=0; j<8; j++){
                this->sprite[i][j] = (this->chr[i]&(1<<k))!=0;
                k--;
            }
        }
        for(int i=8; i<16; i++){
            int k = 7;
            for(int j=0; j<8; j++){
                this->sprite[i-8][j] |= ((this->chr[i]&(1<<k))!=0)<<1;
                k--;
            }
        }
        if((sprite_info->attr&0x40)!=0){
            this->FlipSpriteHorizontally();
        }
        if((sprite_info->attr&0x80)!=0){
            this->FlipSpriteVertically();
        }
        palette_idx = sprite_info->attr&0x03;
        palette_idx = palette_idx<<2;
        uint32_t palette = this->GetSpritePalette(palette_idx);
        int y;
        Pixel color;
        uint16_t sprite_data[8*8];
        memset(sprite_data, 0, 8*8*2);
        for(int dy=0; dy<8; dy++){
            for(int dx=0; dx<8; dx++){
                y = dy+sprite_info->y+1;
                if((y)>=240){
                    continue;
                }
                int _xpos = sprite_info->x+dx;
                bool is_transparent = false; // (this->transparent_buff[y][_xpos/8] & (1 << _xpos %8));
                if((!(sprite_info->attr&0x20))||is_transparent){
                    if(!this->sprite[dy][dx]){
                        continue;
                    }
                    if((((int)sprite_info->x)+dx)>=WIDTH){
                        continue;
                    }
                    int _p = *(((char*)&palette)+this->sprite[dy][dx]);
                    color = this->palettes[_p];
                    // color = this->palettes[(int)(*(((char*)&palette)+this->sprite[dy][dx]))];
                    // this->gui->SetPixel(sprite_info->x+dx, y, color);
                    uint16_t packed_color = make_color(color.b, color.g, color.r);
                    // set_pixel(sprite_info->x+dx, y, packed_color);
                    sprite_data[dx + 8*dy] = packed_color;
                }
            }
        }
        lcd_write_frame(sprite_info->x, sprite_info->y+1, 8, 8, (const uint8_t*)sprite_data);
    }
    
}

uint16_t Ppu::GetTileAddr(int x, int y){
    uint32_t temp_x, temp_y;
    temp_x = (x+this->GetScrollX());
    temp_y = (y+this->GetScrollY());
    x = temp_x%WIDTH;
    y = temp_y%HEIGHT;
    int tx, ty;
    tx = x>>5;
    ty = y>>5;
    uint8_t p[sizeof(uint32_t)];
    uint8_t palette_idx;
    uint16_t tile_addr;
 
    uint32_t name_table_id;
    name_table_id = (temp_x/WIDTH)%2 + (((temp_y/HEIGHT)%2) ? 2 : 0);
    tile_addr = 0x23C0+name_table_id*0x400+tx+ty*8;

    if(this->horizontal_mirror){
        if(0x27C0<=tile_addr && tile_addr<=0x27FF){
            tile_addr = tile_addr - 0x400;
        }else if(0x2FC0<=tile_addr && tile_addr<=0x2FFF){
            tile_addr = tile_addr - 0x400;
        }
    }else{
        if(tile_addr>=0x2800&&tile_addr<=0x2BFF){
            tile_addr = tile_addr - 0x800;
        }else if(tile_addr>=0x2C00&&tile_addr<=0x2FFF){
            tile_addr = tile_addr - 0x800;
        }
    }
    return tile_addr;
}

uint16_t Ppu::GetBlockId(int x, int y){
    x = (x+this->GetScrollX())%WIDTH;
    y = (y+this->GetScrollY())%HEIGHT;
    uint16_t block_id = (x>>4)%2 + (((y>>4)%2) ? 2:0);
    return block_id;
}

uint32_t Ppu::GetPalette(uint16_t tile_addr, uint16_t block_id){
    uint8_t palette_idx;
    uint8_t p[sizeof(uint32_t)];
    uint32_t palette;
    palette_idx = (this->vram.raw[tile_addr]>>(block_id*2))&0x03;
    for(int i=0; i<sizeof(uint32_t); i++){
        int idx = 0x3F00+4*palette_idx+i;
        if(idx%4==0){
            *(p+i) = this->vram.raw[0x3F00];
            continue;
        }
        *(p+i) = this->vram.raw[idx];
    }
    palette = *((uint32_t*)p);   
    return palette;
}

void Ppu::DrawBg(){
    uint32_t palette;
    int y = this->line-1;
    uint16_t line_data[WIDTH];
    for(int x=0; x<WIDTH;){
        int offset_y = ((y+this->GetScrollY())%HEIGHT)%8;
        int offset_x = ((x+this->GetScrollX())%WIDTH)%8;
        this->chr  = this->GetBg(this->GetChrIdx(x,y));
        int j = 7;
        for(int i=0; i<8; i++){
            this->sprite[offset_y][i]    = (this->chr[offset_y]&(1<<j))!=0;
            this->sprite[offset_y][i] |= ((this->chr[8+offset_y]&(1<<j))!=0)<<1;
            j--;
        }
        palette = this->GetPalette(this->GetTileAddr(x, y), this->GetBlockId(x, y));
        // we use 16 bit color...
        uint16_t data[8];
        for(;x<WIDTH&&offset_x<8; offset_x++){
            /*
            if(!this->sprite[offset_y][offset_x]){
               this->transparent_buff[y][x/8] |= (1 << (x % 8));
            }
            */
            // pixel is ARGB
            int _p = *(((char*)&palette)+this->sprite[offset_y][offset_x]);
            Pixel color = this->palettes[_p];
            // Pixel color = this->palettes[(int)(*(((char*)&palette)+this->sprite[offset_y][offset_x]))];
            // this->gui->SetPixel(x, y, color);
            line_data[x] = make_color(color.b, color.g, color.r);
            x++;
        }
    }
    // now send line
    lcd_write_frame(0, y, WIDTH, 1, (const uint8_t*)line_data);
    /***
    for(int idx=0; idx<this->sprite_list.size(); idx++){
        Sprite* sprite_info = &this->sprite_ram.sprites[this->sprite_list[idx]];
        this->chr = this->GetSprite(sprite_info->tile_id);
        int offset_y = y%8;
        int offset_x = sprite_info->x%8;
        int j = 7;
        for(int i=0; i<8; i++){
            this->sprite[offset_y][i]    = (this->chr[offset_y]&(1<<j))!=0;
            this->sprite[offset_y][i] |= ((this->chr[8+offset_y]&(1<<j))!=0)<<1;
            j--;
        }
        uint32_t palette = this->GetSpritePalette((sprite_info->attr&0x03)<<2);
        for(int dx=0; dx<8; dx++){
            if((!(sprite_info->attr&0x20))||(this->transparent_buff[y][sprite_info->x+dx])){
                if(!this->sprite[offset_y][dx]){
                    continue;
                }
                if((((int)sprite_info->x)+dx)>=WIDTH){
                    continue;
                }
                this->gui->SetPixel(sprite_info->x+dx, y, this->palettes[*(((char*)&palette)+this->sprite[offset_y][dx])]);
            }
        }
    }
    ***/
    return;
}

void Ppu::ResetTransparentBuff(){
    /*
    for(int y=0; y<HEIGHT; y++){
        for(int x=0; x<WIDTH/8; x++){
            this->transparent_buff[y][x] = 0;
        }
    }
    */
}

void Ppu::SetVblank(){
    this->registers[PPUSTATUS_KIND] |= 0x80;
}

void Ppu::ClearVblank(){
    this->registers[PPUSTATUS_KIND] &= ~0x80;
}

void Ppu::WriteSprite(int i, uint8_t data){
    this->sprite_ram.raw[i] = data;
}

void Ppu::ShowPalette(){
    for(int i=0; i<32; i++){
        fprintf(stderr, "%02X ", this->vram.raw[0x3F00+i]);
        if((i+1)%16==0){
            cout << endl;
        }
    }
}

void Ppu::WriteScroll(){
    if(this->scroll_x_flg){
        this->scroll_x = this->registers[PPUSCROLL_KIND];
        this->scroll_x_flg = false;
    }else{
        this->scroll_y = this->registers[PPUSCROLL_KIND];
        this->scroll_x_flg = true;
    }
}

uint8_t Ppu::ReadStatus(){
    uint8_t data;
    this->scroll_x_flg = true;
    this->lower_byte   = false;
    data = this->registers[PPUSTATUS_KIND];
    this->ClearVblank();
    return data;
}

void Ppu::ShowNameTable(){
    fprintf(stderr, "Name Table \n\n");
    for(int i=0; i<0x03C0; i++){
        fprintf(stderr, "%02X ", this->vram.raw[0x2000+i]);
        if((i+1)%16==0){
            cout << endl;
        }
    }
    cout << endl;
}

uint32_t Ppu::GetScrollX(){
    return (this->scroll_x + ((this->name_table_id%2)*WIDTH));
}

uint32_t Ppu::GetScrollY(){
    return (this->scroll_y + ((this->name_table_id/2)*HEIGHT));
}

void Ppu::PrintBlockId(){
    for(int y=0; y<HEIGHT; y+=8){
        for(int x=0; x<WIDTH; x+=8){
            fprintf(stderr, "%d ", (x/16)%2 + (((y/16)%2) ? 2:0));
        }
        cout << endl;
    }
}

bool Ppu::IsSprite0(){
    //https://wiki.nesdev.com/w/index.php/PPU_OAM#Sprite_zero_hits
    return this->sprite_ram.sprites[0].y==(this->line-7);
}

void Ppu::SetSprite0(){
    this->registers[PPUSTATUS_KIND] |= 0x40;
}

void Ppu::ClearSprite0(){
    this->registers[PPUSTATUS_KIND] &= ~0x40;
}

void Ppu::SearchSprite(){
    for(int i=0; i<64; i++){
        // this->sprite_ram.sprites[i];
        if((this->sprite_ram.sprites[i].y+8)<=this->line){
            this->sprite_list.push_back(i);
        }
        if(this->sprite_list.size()==8){
            break;
        }
    }
}
