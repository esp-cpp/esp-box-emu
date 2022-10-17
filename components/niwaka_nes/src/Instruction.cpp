#include "Instruction.hpp"
#include "Cpu.hpp"

InstructionBase::InstructionBase(string name, int nbytes, int cycles){
    this->name = name;
    this->nbytes = nbytes;
    this->cycles = cycles;
}

int InstructionBase::GetCycle(){
    return this->cycles;
}

Sei::Sei(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Sei::Execute(Cpu* cpu){
    cpu->SetI();
    return this->cycles;
}

LdxImmediate::LdxImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdxImmediate::Execute(Cpu* cpu){
    uint8_t value = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    cpu->Set8(X_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

LdyImmediate::LdyImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdyImmediate::Execute(Cpu* cpu){
    uint8_t value = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    cpu->Set8(Y_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

Txs::Txs(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Txs::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    cpu->Set8(S_KIND, x_value);
    return this->cycles;
}

Tsx::Tsx(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Tsx::Execute(Cpu* cpu){
    uint8_t s_value = cpu->GetGprValue(S_KIND);
    cpu->Set8(X_KIND, s_value);
    cpu->UpdateNflg(s_value);
    cpu->UpdateZflg(s_value);
    return this->cycles;
}

Tax::Tax(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Tax::Execute(Cpu* cpu){
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    cpu->Set8(X_KIND, a_value);
    cpu->UpdateNflg(a_value);
    cpu->UpdateZflg(a_value);
    return this->cycles;
}

Txa::Txa(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Txa::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    cpu->Set8(A_KIND, x_value);
    cpu->UpdateNflg(x_value);
    cpu->UpdateZflg(x_value);
    return this->cycles;
}

Tya::Tya(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Tya::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    cpu->Set8(A_KIND, y_value);
    cpu->UpdateNflg(y_value);
    cpu->UpdateZflg(y_value);
    return this->cycles;
}

Tay::Tay(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Tay::Execute(Cpu* cpu){
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    cpu->Set8(Y_KIND, a_value);
    cpu->UpdateNflg(a_value);
    cpu->UpdateZflg(a_value);
    return this->cycles;
}

LdaImmediate::LdaImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdaImmediate::Execute(Cpu* cpu){
    uint8_t value = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

StaAbsolute::StaAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StaAbsolute::Execute(Cpu* cpu){
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t addr;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    cpu->Write(addr, a_value);
    return this->cycles;
}

StaAbsoluteX::StaAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StaAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    int cycle = 0;
    addr = cpu->Read16(cpu->GetPc());
    if((x_value+(addr&0x00FF))>0x000000FF){//dummy read
        cpu->Read8((addr&0xFF00)|((x_value+(addr&0x00FF))&0x00FF));
        cycle = 1;
    }
    addr = addr + x_value;
    cpu->Write(addr, cpu->GetGprValue(A_KIND));
    cpu->AddPc(2);
    return this->cycles;
}

StxAbsolute::StxAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StxAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    cpu->Write(addr, cpu->GetGprValue(X_KIND));
    return this->cycles;
}

StaZeropage::StaZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StaZeropage::Execute(Cpu* cpu){
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t addr = 0;
    addr = cpu->Read8(cpu->GetPc())&0x00FF;
    cpu->AddPc(1);
    cpu->Write(addr, a_value);
    return this->cycles;
}

StaIndirectX::StaIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StaIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    cpu->Write(((((uint16_t)cpu->Read8((addr+1)&0x00FF))<<8)|cpu->Read8(addr)), cpu->GetGprValue(A_KIND));
    return this->cycles;
}

StxZeropage::StxZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StxZeropage::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr = 0;
    addr = cpu->Read8(cpu->GetPc())&0x00FF;
    cpu->AddPc(1);
    cpu->Write(addr, x_value);
    return this->cycles;
}

StyZeropage::StyZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StyZeropage::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    addr = cpu->Read8(cpu->GetPc())&0x00FF;
    cpu->AddPc(1);
    cpu->Write(addr, y_value);
    return this->cycles;
}

StaZeropageX::StaZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StaZeropageX::Execute(Cpu* cpu){
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t addr = 0;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    cpu->Write(addr, a_value);
    return this->cycles;
}

LdaAbsoluteX::LdaAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdaAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    int cycle = 0;
    addr = cpu->Read16(cpu->GetPc());
    if((x_value+(addr&0x00FF))>0x000000FF){//dummy read
        cpu->Read8((addr&0xFF00)|((x_value+(addr&0x00FF))&0x00FF));
        cycle = 1;
    }
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);
    cpu->Set8(A_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles+cycle;
}

LdaIndirectX::LdaIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdaIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint8_t addr;
    uint8_t value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(cpu->Read8(addr)|((uint16_t)cpu->Read8((addr+1)&0x00FF)<<8));
    cpu->Set8(A_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

LdxAbsoluteY::LdxAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdxAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);
    cpu->Set8(X_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

LdxAbsolute::LdxAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdxAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);
    cpu->Set8(X_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

LdaZeropage::LdaZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdaZeropage::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    cpu->Set8(A_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

LdxZeropage::LdxZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdxZeropage::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    cpu->Set8(X_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

LdaZeropageX::LdaZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdaZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    cpu->Set8(A_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

Inx::Inx(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Inx::Execute(Cpu* cpu){
    uint8_t value = cpu->GetGprValue(X_KIND)+1;
    cpu->Set8(X_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

Iny::Iny(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Iny::Execute(Cpu* cpu){
    uint8_t value = cpu->GetGprValue(Y_KIND)+1;
    cpu->Set8(Y_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

Dey::Dey(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Dey::Execute(Cpu* cpu){
    uint8_t value = cpu->GetGprValue(Y_KIND)-1;
    cpu->Set8(Y_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles; 
}

Dex::Dex(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Dex::Execute(Cpu* cpu){
    uint8_t value = cpu->GetGprValue(X_KIND)-1;
    cpu->Set8(X_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles; 
}

Bne::Bne(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Bne::Execute(Cpu* cpu){
    uint16_t value = (int16_t)((int8_t)cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if(!cpu->IsZflg()){
        cpu->AddPc(value);
        return this->cycles  + 1;
    }
    return this->cycles;
}

Bmi::Bmi(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Bmi::Execute(Cpu* cpu){
    uint16_t value = (int16_t)((int8_t)cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if(cpu->IsNflg()){
        cpu->AddPc(value);
        return this->cycles  + 1;
    }
    return this->cycles;
}

Bvs::Bvs(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Bvs::Execute(Cpu* cpu){
    uint16_t value = (int16_t)((int8_t)cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if(cpu->IsVflg()){
        cpu->AddPc(value);
        return this->cycles  + 1;
    }
    return this->cycles;
}

Bvc::Bvc(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Bvc::Execute(Cpu* cpu){
    uint16_t value = (int16_t)((int8_t)cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if(!cpu->IsVflg()){
        cpu->AddPc(value);
        return this->cycles  + 1;
    }
    return this->cycles;
}

Bcc::Bcc(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Bcc::Execute(Cpu* cpu){
    uint16_t value = (int16_t)((int8_t)cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if(!cpu->IsCflg()){
        cpu->AddPc(value);
        return this->cycles  + 1;
    }
    return this->cycles;
}

Bcs::Bcs(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Bcs::Execute(Cpu* cpu){
    uint16_t value = (int16_t)((int8_t)cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if(cpu->IsCflg()){
        cpu->AddPc(value);
        return this->cycles  + 1;
    }
    return this->cycles;
}

Beq::Beq(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Beq::Execute(Cpu* cpu){
    uint16_t value = (int16_t)((int8_t)cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if(cpu->IsZflg()){
        cpu->AddPc(value);
        return this->cycles  + 1;
    }
    return this->cycles;
}

BplRelative::BplRelative(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int BplRelative::Execute(Cpu* cpu){
    uint16_t value = (int16_t)((int8_t)cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if(!cpu->IsNflg()){
        cpu->AddPc(value);
        return this->cycles  + 1;
    }
    return this->cycles;
}

JmpAbsolute::JmpAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int JmpAbsolute::Execute(Cpu* cpu){
    uint16_t addr = cpu->Read16(cpu->GetPc());
    cpu->SetPc(addr);
    return this->cycles;
}

LdaAbsolute::LdaAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdaAbsolute::Execute(Cpu* cpu){
    uint16_t addr = cpu->Read16(cpu->GetPc());
    uint8_t value = cpu->Read8(addr);
    cpu->AddPc(2);    
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

CmpImmediate::CmpImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CmpImmediate::Execute(Cpu* cpu){
    uint16_t result = cpu->GetGprValue(A_KIND)-cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }

    return this->cycles;
}

CmpIndirectX::CmpIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CmpIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(cpu->Read8(addr)|((uint16_t)cpu->Read8((addr+1)&0x00FF)<<8));

    uint16_t result = cpu->GetGprValue(A_KIND)-value;
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }

    return this->cycles;
}

CpxImmediate::CpxImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CpxImmediate::Execute(Cpu* cpu){
    uint16_t result = cpu->GetGprValue(X_KIND)-cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }

    /***
    if(result==0){
        cpu->SetC();
        cpu->SetZ();
        cpu->ClearN();
    }else if(result > 0){
        cpu->ClearZ();
        if((result&0x80000000)!=0){
            cpu->SetN();
        }else{
            cpu->ClearN();
        }
        cpu->SetC();
    }else{
        if((result&0x80000000)!=0){
            cpu->SetN();
        }else{
            cpu->ClearN();
        }
        cpu->ClearZ();
        cpu->ClearC();
    }
    ***/
    return this->cycles;
}

CpyImmediate::CpyImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CpyImmediate::Execute(Cpu* cpu){
    uint16_t result = cpu->GetGprValue(Y_KIND) - cpu->Read8(cpu->GetPc());
    //uint16_t result = cpu->GetGprValue(Y_KIND)-cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    /***
    if(result==0){
        cpu->SetC();
        cpu->SetZ();
        cpu->ClearN();
    }else if(result > 0){
        cpu->ClearZ();
        if((result&0x80000000)!=0){
            cpu->SetN();
        }else{
            cpu->ClearN();
        }
        cpu->SetC();
    }else{
        if((result&0x80000000)!=0){
            cpu->SetN();
        }else{
            cpu->ClearN();
        }
        cpu->ClearZ();
        cpu->ClearC();
    }
    ***/
    return this->cycles;
}

AndImmediate::AndImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AndImmediate::Execute(Cpu* cpu){
    uint8_t a_value;
    uint8_t value;
    uint8_t result;
    a_value = cpu->GetGprValue(A_KIND);
    value   = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    result = a_value&value;
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

AndIndirectX::AndIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AndIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t result;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    result = cpu->GetGprValue(A_KIND)&cpu->Read8(cpu->Read8(addr)|((uint16_t)cpu->Read8((addr+1)&0x00FF)<<8));
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

IncAbsolute::IncAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IncAbsolute::Execute(Cpu* cpu){
    uint16_t addr = cpu->Read16(cpu->GetPc());
    uint8_t value = cpu->Read8(addr);
    cpu->AddPc(2);
    value = value + 1;
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

IncZeropage::IncZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IncZeropage::Execute(Cpu* cpu){
    uint16_t addr = 0x00FF & cpu->Read8(cpu->GetPc());
    uint8_t value = cpu->Read8(addr);
    cpu->AddPc(1);
    value = value + 1;
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

DecAbsolute::DecAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DecAbsolute::Execute(Cpu* cpu){
    uint16_t addr = cpu->Read16(cpu->GetPc());
    uint8_t value = cpu->Read8(addr);
    cpu->AddPc(2);
    value = value - 1;
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

DecZeropageX::DecZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DecZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc()))&0x00FF;
    uint8_t value = cpu->Read8(addr);
    cpu->AddPc(1);
    value = value - 1;
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

DecZeropage::DecZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DecZeropage::Execute(Cpu* cpu){
    uint16_t addr;
    addr = cpu->Read8(cpu->GetPc())&0x00FF;
    uint8_t value = cpu->Read8(addr);
    cpu->AddPc(1);
    value = value - 1;
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

AdcImmediate::AdcImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AdcImmediate::Execute(Cpu* cpu){
    uint8_t value   = cpu->Read8(cpu->GetPc());
    uint8_t carry   = cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->AddPc(1);
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

AdcIndirectX::AdcIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AdcIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t result;
    uint8_t value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(cpu->Read16(addr));
    uint8_t carry   = cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    result  = value + a_value + carry;

    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

AdcZeropage::AdcZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AdcZeropage::Execute(Cpu* cpu){
    uint8_t value   = cpu->Read8(cpu->Read8(cpu->GetPc()));
    uint8_t carry   = cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->AddPc(1);
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

JsrAbsolute::JsrAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int JsrAbsolute::Execute(Cpu* cpu){
    uint16_t addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(1);
    uint16_t  saved_address = cpu->GetPc();
    cpu->Push16(saved_address);
    cpu->SetPc(addr);
    return this->cycles;
}

Rts::Rts(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Rts::Execute(Cpu* cpu){
    uint16_t addr;
    addr = cpu->Pop16();
    cpu->SetPc(addr+1);
    return this->cycles;
}

Rti::Rti(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Rti::Execute(Cpu* cpu){
    cpu->SetP(cpu->Pop8()|0x20);
    cpu->SetPc(cpu->Pop16());//0x8044
    return this->cycles;
}

EorImmediate::EorImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int EorImmediate::Execute(Cpu* cpu){
    uint8_t value = cpu->Read8(cpu->GetPc());
    uint8_t result;
    cpu->AddPc(1); 
    result = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

EorIndirectX::EorIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int EorIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t value;
    uint8_t result;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(cpu->Read16(addr));

    result = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

Cld::Cld(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Cld::Execute(Cpu* cpu){
    cpu->ClearD();
    return this->cycles;
}

Clc::Clc(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Clc::Execute(Cpu* cpu){
    cpu->ClearC();
    return this->cycles;
}

Pha::Pha(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Pha::Execute(Cpu* cpu){
    cpu->Push8(cpu->GetGprValue(A_KIND));
    return this->cycles;
}

Php::Php(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Php::Execute(Cpu* cpu){
    //cpu->SetB();
    cpu->Push8(cpu->GetP());
    return this->cycles;
}

Pla::Pla(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Pla::Execute(Cpu* cpu){
    uint8_t value = cpu->Pop8();// | ((cpu->CmpLastInstructionName("Php"))? 0x10 : 0x00);
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

Plp::Plp(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Plp::Execute(Cpu* cpu){
    cpu->SetP(cpu->Pop8()|0x20);
    return this->cycles;
}

Sec::Sec(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Sec::Execute(Cpu* cpu){
    cpu->SetC();
    return this->cycles;
}

SbcZeropage::SbcZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SbcZeropage::Execute(Cpu* cpu){
    uint8_t value   = cpu->Read8(cpu->Read8(cpu->GetPc()));
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->AddPc(1);
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    //if((((a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

SbcImmediate::SbcImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SbcImmediate::Execute(Cpu* cpu){
    uint8_t value   = cpu->Read8(cpu->GetPc());
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->AddPc(1);
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    //if((((a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

SbcIndirectX::SbcIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SbcIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(cpu->Read16(addr));

    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    //if((((a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

OraImmediate::OraImmediate(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int OraImmediate::Execute(Cpu* cpu){
    uint8_t a_value = cpu->GetGprValue(A_KIND) | cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    cpu->Set8(A_KIND, a_value);

    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

OraAbsoluteX::OraAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int OraAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);
    uint8_t a_value = cpu->GetGprValue(A_KIND) | value;

    cpu->Set8(A_KIND, a_value);
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

OraAbsolute::OraAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int OraAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);

    uint8_t a_value = cpu->GetGprValue(A_KIND) | value;
    
    cpu->Set8(A_KIND, a_value);
    
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

OraIndirectX::OraIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int OraIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(cpu->Read16(addr));

    uint8_t a_value = cpu->GetGprValue(A_KIND) | value;
    
    cpu->Set8(A_KIND, a_value);
    
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

LsrAccumulator::LsrAccumulator(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LsrAccumulator::Execute(Cpu* cpu){
    if((cpu->GetGprValue(A_KIND)&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    uint8_t a_value = cpu->GetGprValue(A_KIND) >> 1;
    cpu->Set8(A_KIND, a_value);

    cpu->ClearN();
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

AslAccumulator::AslAccumulator(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AslAccumulator::Execute(Cpu* cpu){
    if((cpu->GetGprValue(A_KIND)&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    uint8_t a_value = cpu->GetGprValue(A_KIND) << 1;
    cpu->Set8(A_KIND, a_value);

    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

AslAbsoluteX::AslAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AslAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);

    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value<<1;

    cpu->Write(addr, value);

    if((value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

Nop::Nop(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Nop::Execute(Cpu* cpu){
    return this->cycles;
}

BitZeropage::BitZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int BitZeropage::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t result;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc())&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    if((value&0x40)!=0){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value&cpu->GetGprValue(A_KIND));
    return this->cycles;
}

Sed::Sed(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Sed::Execute(Cpu* cpu){
    cpu->SetD();
    return this->cycles;
}

Clv::Clv(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int Clv::Execute(Cpu* cpu){
    cpu->ClearV();
    return this->cycles;
}

RorAccumulator::RorAccumulator(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RorAccumulator::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    if((a_value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    a_value = a_value >> 1;
    a_value = a_value | ((carry)? 0x80:0x00);
    cpu->Set8(A_KIND, a_value);
    cpu->UpdateNflg(a_value);
    cpu->UpdateZflg(a_value);
    return this->cycles;
}

RolAccumulator::RolAccumulator(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RolAccumulator::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    if((a_value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    a_value = a_value << 1;
    a_value = a_value | ((carry)? 0x01:0x00);
    cpu->Set8(A_KIND, a_value);
    cpu->UpdateNflg(a_value);
    cpu->UpdateZflg(a_value);
    return this->cycles;
}

LdyZeropage::LdyZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdyZeropage::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    cpu->Set8(Y_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

OraZeropage::OraZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int OraZeropage::Execute(Cpu* cpu){
    uint8_t a_value = cpu->GetGprValue(A_KIND) | cpu->Read8(cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    cpu->Set8(A_KIND, a_value);
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

AndZeropage::AndZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AndZeropage::Execute(Cpu* cpu){
    uint8_t result;
    result = cpu->GetGprValue(A_KIND)&cpu->Read8(cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

EorZeropage::EorZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int EorZeropage::Execute(Cpu* cpu){
    uint8_t result;
    result = cpu->GetGprValue(A_KIND)^cpu->Read8(cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1); 
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

CmpZeropage::CmpZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CmpZeropage::Execute(Cpu* cpu){
    uint16_t result = cpu->GetGprValue(A_KIND)-cpu->Read8(cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }

    return this->cycles;
}

CpxZeropage::CpxZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CpxZeropage::Execute(Cpu* cpu){
    uint16_t result = cpu->GetGprValue(X_KIND)-cpu->Read8(cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

CpyZeropage::CpyZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CpyZeropage::Execute(Cpu* cpu){
    uint16_t result = cpu->GetGprValue(Y_KIND)-cpu->Read8(cpu->Read8(cpu->GetPc()));
    cpu->AddPc(1);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

LsrZeropage::LsrZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LsrZeropage::Execute(Cpu* cpu){
    uint8_t addr = cpu->Read8(cpu->GetPc());
    uint8_t value = cpu->Read8(addr);
    cpu->AddPc(1);
    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write((uint16_t)addr, value);
    cpu->ClearN();
    if(value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

AslZeropage::AslZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AslZeropage::Execute(Cpu* cpu){
    uint8_t addr = cpu->Read8(cpu->GetPc());
    uint8_t value = cpu->Read8(addr);
    cpu->AddPc(1);
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    cpu->Write((uint16_t)addr, value);
    if((value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}


RorZeropage::RorZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RorZeropage::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();
    uint8_t addr = cpu->Read8(cpu->GetPc());
    uint8_t value = cpu->Read8(addr);
    cpu->AddPc(1);
    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    value = value | ((carry)? 0x80:0x00);
    cpu->Write((uint16_t)addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

RolZeropage::RolZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RolZeropage::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();
    uint8_t addr = cpu->Read8(cpu->GetPc());
    uint8_t value = cpu->Read8(addr);
    cpu->AddPc(1);
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write((uint16_t)addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

LdyAbsolute::LdyAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdyAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);
    cpu->Set8(Y_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

StyAbsolute::StyAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StyAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    cpu->Write(addr, cpu->GetGprValue(Y_KIND));
    return this->cycles;
}

BitAbsolute::BitAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int BitAbsolute::Execute(Cpu* cpu){
    uint8_t value;
    uint8_t result;
    value = cpu->Read8(cpu->Read16(cpu->GetPc()));
    cpu->AddPc(2);

    if((value&0x40)!=0){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value&cpu->GetGprValue(A_KIND));
    return this->cycles;
}

AndAbsolute::AndAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AndAbsolute::Execute(Cpu* cpu){
    uint8_t value;
    uint8_t result;
    value = cpu->Read8(cpu->Read16(cpu->GetPc()));
    cpu->AddPc(2);
    result = cpu->GetGprValue(A_KIND)&value;
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

EorAbsolute::EorAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int EorAbsolute::Execute(Cpu* cpu){
    uint8_t result;
    result = cpu->GetGprValue(A_KIND)^cpu->Read8(cpu->Read16(cpu->GetPc()));
    cpu->AddPc(2);
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

AdcAbsolute::AdcAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AdcAbsolute::Execute(Cpu* cpu){
    uint8_t value   = cpu->Read8(cpu->Read16(cpu->GetPc()));
    uint8_t carry   = cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->AddPc(2);
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

CmpAbsolute::CmpAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CmpAbsolute::Execute(Cpu* cpu){
    uint16_t result = cpu->GetGprValue(A_KIND)-cpu->Read8(cpu->Read16(cpu->GetPc()));
    cpu->AddPc(2);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }

    return this->cycles;
}

SbcAbsolute::SbcAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SbcAbsolute::Execute(Cpu* cpu){
    uint8_t value   = cpu->Read8(cpu->Read16(cpu->GetPc()));
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->AddPc(2);
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    //if((((a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

CpxAbsolute::CpxAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CpxAbsolute::Execute(Cpu* cpu){
    uint16_t result = cpu->GetGprValue(X_KIND)-cpu->Read8(cpu->Read16(cpu->GetPc()));
    cpu->AddPc(2);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

CpyAbsolute::CpyAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CpyAbsolute::Execute(Cpu* cpu){
    uint16_t result = cpu->GetGprValue(Y_KIND)-cpu->Read8(cpu->Read16(cpu->GetPc()));
    cpu->AddPc(2);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

LsrAbsolute::LsrAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LsrAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);
    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write(addr, value);
    cpu->ClearN();
    if(value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

AslAbsolute::AslAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AslAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    cpu->Write(addr, value);
    if((value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

RorAbsolute::RorAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RorAbsolute::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);
    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    value = value | ((carry)? 0x80:0x00);
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

RolAbsolute::RolAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RolAbsolute::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write((uint16_t)addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

LdaIndirectY::LdaIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdaIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);
    cpu->Set8(A_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

OraIndirectY::OraIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int OraIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);

    uint8_t a_value = cpu->GetGprValue(A_KIND) | value;
    cpu->Set8(A_KIND, a_value);

    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

AndIndirectY::AndIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AndIndirectY::Execute(Cpu* cpu){
    uint8_t a_value;
    uint8_t result;
    uint8_t value;
    a_value = cpu->GetGprValue(A_KIND);

    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);

    result = a_value&value;
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

EorIndirectY::EorIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int EorIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t result;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);

    result = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

AdcIndirectY::AdcIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AdcIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);
    uint8_t carry   = cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

CmpIndirectY::CmpIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CmpIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);

    uint16_t result = cpu->GetGprValue(A_KIND)-value;
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }

    return this->cycles;
}

SbcIndirectY::SbcIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SbcIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);

    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;

    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    //if((((a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

StaIndirectY::StaIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StaIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;

    cpu->Write(addr, cpu->GetGprValue(A_KIND));
    return this->cycles;
}

JmpIndirect::JmpIndirect(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int JmpIndirect::Execute(Cpu* cpu){
    uint16_t addr;
    addr = cpu->Read16(cpu->GetPc());
    addr = (((uint16_t)cpu->Read8((addr&0xFF00)|((addr+1)&0x00FF)))<<8)|(cpu->Read8(addr));
    cpu->SetPc(addr);
    return this->cycles;
}

RorAbsoluteX::RorAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RorAbsoluteX::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();

    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);

    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    value = value | ((carry)? 0x80:0x00);
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

LdaAbsoluteY::LdaAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdaAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)(cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);
    cpu->Set8(A_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

OraAbsoluteY::OraAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int OraAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);
    uint8_t a_value = cpu->GetGprValue(A_KIND) | value;

    cpu->Set8(A_KIND, a_value);
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

AndAbsoluteY::AndAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AndAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);

    uint8_t a_value;
    uint8_t result;
    a_value = cpu->GetGprValue(A_KIND);
    result = a_value&value;
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

EorAbsoluteY::EorAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int EorAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);

    uint8_t result;

    result = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

AdcAbsoluteY::AdcAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AdcAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);

    uint8_t carry   = cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;

    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

CmpAbsoluteY::CmpAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CmpAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);

    uint16_t result = cpu->GetGprValue(A_KIND)-value;

    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }

    return this->cycles;
}

SbcAbsoluteY::SbcAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SbcAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);

    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;

    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    //if((((a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

StaAbsoluteY::StaAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StaAbsoluteY::Execute(Cpu* cpu){
    cpu->Write(cpu->Read16(cpu->GetPc()) + (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND)), cpu->GetGprValue(A_KIND));
    cpu->AddPc(2);
    return this->cycles;
}

LdyZeropageX::LdyZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdyZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    cpu->Set8(Y_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

StyZeropageX::StyZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StyZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    cpu->Write(addr, cpu->GetGprValue(Y_KIND));
    return this->cycles;
}

OraZeropageX::OraZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int OraZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    uint8_t a_value = cpu->GetGprValue(A_KIND) | value;

    cpu->Set8(A_KIND, a_value);

    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

AndZeropageX::AndZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AndZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    uint8_t a_value;
    uint8_t result;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    a_value = cpu->GetGprValue(A_KIND);
    result = a_value&value;
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

EorZeropageX::EorZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int EorZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    uint8_t result;

    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    result = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

AdcZeropageX::AdcZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AdcZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    uint8_t carry   = cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;

    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

CmpZeropageX::CmpZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CmpZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    uint16_t result = cpu->GetGprValue(A_KIND)-value;

    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }

    return this->cycles;
}

SbcZeropageX::SbcZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SbcZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;

    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    //if((((a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

LsrZeropageX::LsrZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LsrZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    
    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write((uint16_t)addr, value);
    cpu->ClearN();
    if(value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

AslZeropageX::AslZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AslZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    cpu->Write(addr, value);
    if((value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

RorZeropageX::RorZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RorZeropageX::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    value = value | ((carry)? 0x80:0x00);
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

RolZeropageX::RolZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RolZeropageX::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write((uint16_t)addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

IncZeropageX::IncZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IncZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    value = value + 1;
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

LdxZeropageY::LdxZeropageY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdxZeropageY::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(Y_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    cpu->Set8(X_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

StxZeropageY::StxZeropageY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int StxZeropageY::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr = 0;
    addr = (cpu->GetGprValue(Y_KIND)+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    cpu->Write(addr, x_value);
    return this->cycles;
}

LdyAbsoluteX::LdyAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LdyAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);
    cpu->Set8(Y_KIND, value);  
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

AndAbsoluteX::AndAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AndAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t result;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    result = cpu->GetGprValue(A_KIND)&cpu->Read8(addr);
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

EorAbsoluteX::EorAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int EorAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t result;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    result = cpu->GetGprValue(A_KIND)^cpu->Read8(addr);
    cpu->Set8(A_KIND, result);
    cpu->UpdateNflg(result);
    cpu->UpdateZflg(result);
    return this->cycles;
}

AdcAbsoluteX::AdcAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int AdcAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);
    uint8_t carry   = cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

CmpAbsoluteX::CmpAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int CmpAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    uint16_t result = cpu->GetGprValue(A_KIND)-cpu->Read8(addr);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    
    if((result&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }

    if(result==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }

    return this->cycles;
}

SbcAbsoluteX::SbcAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SbcAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;

    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    //if((((a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

LsrAbsoluteX::LsrAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LsrAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);
    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write(addr, value);
    cpu->ClearN();
    if(value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

RolAbsoluteX::RolAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RolAbsoluteX::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();

    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);

    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write((uint16_t)addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

IncAbsoluteX::IncAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IncAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);
    value = value + 1;
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

DecAbsoluteX::DecAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DecAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);
    value = value - 1;
    cpu->Write(addr, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

NopD::NopD(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int NopD::Execute(Cpu* cpu){
    cpu->AddPc(1);
    return this->cycles;
}

NopAx::NopAx(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int NopAx::Execute(Cpu* cpu){
    cpu->AddPc(2);
    return this->cycles;
}

NopDx::NopDx(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int NopDx::Execute(Cpu* cpu){
    cpu->AddPc(1);
    return this->cycles;
}

LaxIndirectX::LaxIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LaxIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint8_t addr;
    uint8_t value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(cpu->Read8(addr)|((uint16_t)cpu->Read8((addr+1)&0x00FF)<<8));
    cpu->Set8(A_KIND, value);  
    cpu->Set8(X_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

LaxZeropage::LaxZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LaxZeropage::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    cpu->Set8(A_KIND, value);  
    cpu->Set8(X_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

LaxAbsolute::LaxAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LaxAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);
    cpu->Set8(A_KIND, value);  
    cpu->Set8(X_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

LaxIndirectY::LaxIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LaxIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);
    cpu->Set8(A_KIND, value);  
    cpu->Set8(X_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

LaxZeropageY::LaxZeropageY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LaxZeropageY::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(Y_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    cpu->Set8(A_KIND, value);  
    cpu->Set8(X_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

LaxAbsoluteY::LaxAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int LaxAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);
    cpu->Set8(A_KIND, value);  
    cpu->Set8(X_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value); 
    return this->cycles;
}

SaxIndirectX::SaxIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SaxIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    cpu->Write(((((uint16_t)cpu->Read8((addr+1)&0x00FF))<<8)|cpu->Read8(addr)), cpu->GetGprValue(A_KIND)&cpu->GetGprValue(X_KIND));
    return this->cycles;
}

SaxZeropage::SaxZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SaxZeropage::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr = 0;
    addr = cpu->Read8(cpu->GetPc())&0x00FF;
    cpu->AddPc(1);
    cpu->Write(addr, cpu->GetGprValue(A_KIND)&cpu->GetGprValue(X_KIND));
    return this->cycles;
}

SaxAbsolute::SaxAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SaxAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    cpu->Write(addr, cpu->GetGprValue(X_KIND)&cpu->GetGprValue(A_KIND));
    return this->cycles;
}

SaxZeropageY::SaxZeropageY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SaxZeropageY::Execute(Cpu* cpu){
    uint16_t addr;
    addr = (cpu->GetGprValue(Y_KIND)+cpu->Read8(cpu->GetPc()))&0x00FF;
    cpu->AddPc(1);
    cpu->Write(addr, cpu->GetGprValue(A_KIND)&cpu->GetGprValue(X_KIND));
    return this->cycles;
}


DcpIndirectX::DcpIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DcpIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    addr = cpu->Read8(addr)|((uint16_t)cpu->Read8((addr+1)&0x00FF)<<8);
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    value = value - 1;
    cpu->Write(addr, value);
    uint16_t result  = cpu->GetGprValue(A_KIND) - value;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    return this->cycles;
}

DcpZeropage::DcpZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DcpZeropage::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc())&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    value = value - 1;
    cpu->Write(addr, value);
    uint16_t result  = cpu->GetGprValue(A_KIND) - value;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    return this->cycles;
}

DcpAbsolute::DcpAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DcpAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);

    value = value - 1;
    cpu->Write(addr, value);
    uint16_t result  = cpu->GetGprValue(A_KIND) - value;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    return this->cycles;
}

DcpIndirectY::DcpIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DcpIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);

    value = value - 1;
    cpu->Write(addr, value);
    uint16_t result  = cpu->GetGprValue(A_KIND) - value;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    return this->cycles;
}

DcpZeropageX::DcpZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DcpZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    value = value - 1;
    cpu->Write(addr, value);
    uint16_t result  = cpu->GetGprValue(A_KIND) - value;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    return this->cycles;
}

DcpAbsoluteY::DcpAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DcpAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);

    value = value - 1;
    cpu->Write(addr, value);
    uint16_t result  = cpu->GetGprValue(A_KIND) - value;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    return this->cycles;
}

DcpAbsoluteX::DcpAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int DcpAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);

    value = value - 1;
    cpu->Write(addr, value);
    uint16_t result  = cpu->GetGprValue(A_KIND) - value;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    return this->cycles;
}

IscIndirectX::IscIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IscIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    addr = cpu->Read16(addr);
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    value = value + 1;
    cpu->Write(addr, value);
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

IscZeropage::IscZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IscZeropage::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    value = value + 1;
    cpu->Write(addr, value);
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

IscAbsolute::IscAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IscAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);
    value = value + 1;
    cpu->Write(addr, value);
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

IscIndirectY::IscIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IscIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);

    value = value + 1;
    cpu->Write(addr, value);
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

IscZeropageX::IscZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IscZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    
    value = value + 1;
    cpu->Write(addr, value);
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

IscAbsoluteY::IscAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IscAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);
    
    value = value + 1;
    cpu->Write(addr, value);
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

IscAbsoluteX::IscAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int IscAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);
    
    value = value + 1;
    cpu->Write(addr, value);
    uint8_t carry   = !cpu->GetCFLg();
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint16_t result  = a_value - value - carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if((result&0x00000100)==0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if((((a_value ^ value)&0x80)!=0)&&((((a_value ^ result)&0x80)!=0))){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

SloIndirectX::SloIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SloIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t value;
    uint8_t a_value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    addr = cpu->Read16(addr);
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value<<1;
    cpu->Write(addr, value);

    a_value = cpu->GetGprValue(A_KIND)|value;
    cpu->Set8(A_KIND, a_value);
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

SloZeropage::SloZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SloZeropage::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc())&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value<<1;
    cpu->Write(addr, value);

    uint8_t a_value = cpu->GetGprValue(A_KIND)|value;
    cpu->Set8(A_KIND, a_value);
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

SloAbsolute::SloAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SloAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);

    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value<<1;
    cpu->Write(addr, value);

    uint8_t a_value = cpu->GetGprValue(A_KIND)|value;
    cpu->Set8(A_KIND, a_value);
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

SloIndirectY::SloIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SloIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);


    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value<<1;
    cpu->Write(addr, value);

    uint8_t a_value = cpu->GetGprValue(A_KIND)|value;
    cpu->Set8(A_KIND, a_value);
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

SloZeropageX::SloZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SloZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value<<1;
    cpu->Write(addr, value);

    uint8_t a_value = cpu->GetGprValue(A_KIND)|value;
    cpu->Set8(A_KIND, a_value);
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

SloAbsoluteY::SloAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SloAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);

    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value<<1;
    cpu->Write(addr, value);

    uint8_t a_value = cpu->GetGprValue(A_KIND)|value;
    cpu->Set8(A_KIND, a_value);
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

SloAbsoluteX::SloAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SloAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);

    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value<<1;
    cpu->Write(addr, value);

    uint8_t a_value = cpu->GetGprValue(A_KIND)|value;
    cpu->Set8(A_KIND, a_value);
    if((a_value&0x80)!=0){
        cpu->SetN();
    }else{
        cpu->ClearN();
    }
    if(a_value==0){
        cpu->SetZ();
    }else{
        cpu->ClearZ();
    }
    return this->cycles;
}

RlaIndirectX::RlaIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RlaIndirectX::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();

    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t value;
    uint8_t a_value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    addr = cpu->Read16(addr);
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)&value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

RlaZeropage::RlaZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RlaZeropage::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();

    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)&value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

RlaAbsolute::RlaAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RlaAbsolute::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();

    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);
    
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)&value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

RlaIndirectY::RlaIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RlaIndirectY::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();

    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);
    
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)&value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

RlaZeropageX::RlaZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RlaZeropageX::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();

    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);
    
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)&value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

RlaAbsoluteY::RlaAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RlaAbsoluteY::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();

    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);
    
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)&value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

RlaAbsoluteX::RlaAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RlaAbsoluteX::Execute(Cpu* cpu){
    bool carry = cpu->IsCflg();

    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);
    
    if((value&0x80)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value << 1;
    value = value | ((carry)? 0x01:0x00);
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)&value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

SreIndirectX::SreIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SreIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    addr = cpu->Read8(addr)|((uint16_t)cpu->Read8((addr+1)&0x00FF)<<8);
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

SreZeropage::SreZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SreZeropage::Execute(Cpu* cpu){
    uint8_t value;
    uint16_t addr;
    addr = cpu->Read8(cpu->GetPc())&0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

SreAbsolute::SreAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SreAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);

    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

SreIndirectY::SreIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SreIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);

    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

SreZeropageX::SreZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SreZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

SreAbsoluteY::SreAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SreAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);

    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

SreAbsoluteX::SreAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int SreAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);

    if((value&0x01)!=0){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    value = value >> 1;
    cpu->Write(addr, value);
    value = cpu->GetGprValue(A_KIND)^value;
    cpu->Set8(A_KIND, value);
    cpu->UpdateNflg(value);
    cpu->UpdateZflg(value);
    return this->cycles;
}

RraIndirectX::RraIndirectX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RraIndirectX::Execute(Cpu* cpu){
    uint8_t x_value = cpu->GetGprValue(X_KIND);
    uint16_t addr;
    uint8_t value;
    addr = (x_value+cpu->Read8(cpu->GetPc()))&0x00FF;
    addr = cpu->Read16(addr);
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    uint8_t carry = ((value&0x01)!=0)? 1:0;
    value = value >> 1;
    value = value | ((cpu->IsCflg())? 0x80:0x00);
    cpu->Write(addr, value);
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

RraZeropage::RraZeropage(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RraZeropage::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    uint8_t carry = ((value&0x01)!=0)? 1:0;
    value = value >> 1;
    value = value | ((cpu->IsCflg())? 0x80:0x00);
    cpu->Write(addr, value);
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

RraAbsolute::RraAbsolute(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RraAbsolute::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    value = cpu->Read8(addr);

    uint8_t carry = ((value&0x01)!=0)? 1:0;
    value = value >> 1;
    value = value | ((cpu->IsCflg())? 0x80:0x00);
    cpu->Write(addr, value);
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

RraIndirectY::RraIndirectY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RraIndirectY::Execute(Cpu* cpu){
    uint8_t y_value = cpu->GetGprValue(Y_KIND);
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read8(cpu->GetPc());
    cpu->AddPc(1);
    addr = (((uint16_t)(cpu->Read8((addr+1)&0x00FF)))<<8)|cpu->Read8(addr);
    addr = addr + (uint16_t)y_value;
    value = cpu->Read8(addr);

    uint8_t carry = ((value&0x01)!=0)? 1:0;
    value = value >> 1;
    value = value | ((cpu->IsCflg())? 0x80:0x00);
    cpu->Write(addr, value);
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

RraZeropageX::RraZeropageX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RraZeropageX::Execute(Cpu* cpu){
    uint16_t addr;
    uint8_t value;
    addr = (cpu->GetGprValue(X_KIND)+cpu->Read8(cpu->GetPc())) & 0x00FF;
    cpu->AddPc(1);
    value = cpu->Read8(addr);

    uint8_t carry = ((value&0x01)!=0)? 1:0;
    value = value >> 1;
    value = value | ((cpu->IsCflg())? 0x80:0x00);
    cpu->Write(addr, value);
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

RraAbsoluteY::RraAbsoluteY(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RraAbsoluteY::Execute(Cpu* cpu){
    uint16_t y_value = (int16_t)((uint8_t)cpu->GetGprValue(Y_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + y_value;
    value = cpu->Read8(addr);

    uint8_t carry = ((value&0x01)!=0)? 1:0;
    value = value >> 1;
    value = value | ((cpu->IsCflg())? 0x80:0x00);
    cpu->Write(addr, value);
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}

RraAbsoluteX::RraAbsoluteX(string name, int nbytes, int cycles):InstructionBase(name, nbytes, cycles){

}

int RraAbsoluteX::Execute(Cpu* cpu){
    uint16_t x_value = (int16_t)((uint8_t)cpu->GetGprValue(X_KIND));
    uint16_t addr;
    uint8_t value;
    addr = cpu->Read16(cpu->GetPc());
    cpu->AddPc(2);
    addr = addr + x_value;
    value = cpu->Read8(addr);

    uint8_t carry = ((value&0x01)!=0)? 1:0;
    value = value >> 1;
    value = value | ((cpu->IsCflg())? 0x80:0x00);
    cpu->Write(addr, value);
    uint8_t a_value = cpu->GetGprValue(A_KIND);
    uint8_t result  = value + a_value + carry;
    cpu->UpdateZflg(result);
    cpu->UpdateNflg(result);
    cpu->Set8(A_KIND, result);
    if(((uint32_t)((uint32_t)value + (uint32_t)a_value + (uint32_t)carry))>0xFF){
        cpu->SetC();
    }else{
        cpu->ClearC();
    }
    if(((~(a_value ^ value))&(a_value ^ result)&0x80)!=0x00){
        cpu->SetV();
    }else{
        cpu->ClearV();
    }
    return this->cycles;
}