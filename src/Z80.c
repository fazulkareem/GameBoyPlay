/**
 * PlayEmulator - Simple Gameboy emulator written in C.
 * Copyright (C) 2015 - Aurelien Tran <aurelien.tran@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */


/******************************************************/
/* Include                                            */
/******************************************************/

#include <assert.h>
#include <stdint.h>
#include <Memory.h>
#include <Log.h>


/******************************************************/
/* Macro                                              */
/******************************************************/

/**
 * Get 16 bit register pointer
 * @param reg The 16 bit register Z80_RegName_e id
 */
#define Z80_REG16(reg)  (&Z80_State.Reg[reg])

/**
 * Get 8 bit register pointer
 * @param reg The 8 bit register Z80_RegName_e id
 */
#define Z80_REG8(reg)   (&Z80_State.Reg[(reg) / 2].Byte[(reg) % 2])

/**
 * Concat 2 Byte data to 1 Word
 * @param data0 The first Byte
 * @param data1 The second Byte
 */
#define CONCAT(data0, data1) ((data1) << 8 | (data0))


/******************************************************/
/* Type                                               */
/******************************************************/

/** All purpose 8 bit register type */
typedef union tagReg8_t
{
    uint8_t UByte;  /**< Unsigned 8 bit */
    int8_t  SByte;  /**< Signed 8 bit */
} Reg8_t;

/** All purpose 16 bit register type */
typedef union tagReg16_t
{
    uint16_t UWord;     /**< Unsigned 16 bit access */
    int16_t  SWord;     /**< Signed 16 bit access */
    Reg8_t   Byte[2];   /**< 8 bit access */
} Reg16_t;

/** Z80 Register name */
typedef enum tagZ80_RegName_e
{
    /* 16 bit Register Name */
    Z80_AF = 0,     /**< Accumulator and Flag register */
    Z80_BC,         /**< All purpose register */
    Z80_DE,         /**< All purpose register */
    Z80_HL,         /**< All purpose register */
    Z80_SP,         /**< Stack Pointer register */
    Z80_PC,         /**< Program Pointer register */
    Z80_REG_NUM,    /**< Number of internal register */

    /* 8 bit Register Name */
    Z80_A = 0,      /**< Accumulator register */
    Z80_F,          /**< Flag register */
    Z80_B,          /**< All purpose register */
    Z80_C,          /**< All purpose register */
    Z80_D,          /**< All purpose register */
    Z80_E,          /**< All purpose register */
    Z80_H,          /**< All purpose register */
    Z80_L,          /**< All purpose register */
} Z80_RegName_e;

/** Z80 State */
typedef struct tagZ80_State_t
{
    Reg16_t Reg[Z80_REG_NUM];   /**< Internal Register */
} Z80_State_t;

/* Forward declaration for Z80_ExecuteCallback_t definition */
typedef struct tagZ80_OpCode_t Z80_OpCode_t;

/**
 * Callback to Execute an OpCode
 * @param dst Destination register
 * @param src Source register
 */
typedef int (*Z80_ExecuteCallback_t)(Z80_OpCode_t const * const opcode);

/** Z80 OpCode information */
typedef struct tagZ80_OpCode_t
{
    uint32_t Value;         /**< OpCode Value */
    uint32_t Size;          /**< OpCode Byte size */
    char* Name;             /**< OpCode Name */
    uint32_t Param0;        /**< OpCode Param 1 */
    uint32_t Param1;        /**< Opcode Param 2 */
    Z80_ExecuteCallback_t ExecuteCallback; /**< OpCode execution callback */
} Z80_OpCode_t;


/******************************************************/
/* Prototype                                          */
/******************************************************/

/** @todo replace this function by the real one */
static int Z80_Execute_Unimplemented(Z80_OpCode_t const * const opcode);

/* 16 bit Load/Move/Store Command*/
static int Z80_Execute_LD_RR_NN(Z80_OpCode_t const * const opcode);

/******************************************************/
/* Variable                                           */
/******************************************************/

/** Z80 State */
static Z80_State_t Z80_State;

/** Callback table for each OpCode */
static Z80_OpCode_t const Z80_OpCode[] =
{
    {0x00, 1, "NOP",           0,        0,        Z80_Execute_Unimplemented},
    {0x01, 3, "LD BC,0x%04X\n",Z80_BC,   0,        Z80_Execute_LD_RR_NN},
    {0x02, 1, "LD (BC),A",     Z80_BC,   Z80_A,    Z80_Execute_Unimplemented},
    {0x03, 1, "INC BC",        Z80_BC,   0,        Z80_Execute_Unimplemented},
    {0x04, 1, "INC B",         Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x05, 1, "DEC B",         Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x06, 2, "LD B,d8",       Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x07, 1, "RLCA",          0,        0,        Z80_Execute_Unimplemented},
    {0x08, 3, "LD (a16),SP",   0,        Z80_SP,   Z80_Execute_Unimplemented},
    {0x09, 1, "ADD HL,BC",     Z80_HL,   Z80_BC,   Z80_Execute_Unimplemented},
    {0x0A, 1, "LD A,(BC)",     Z80_A,    Z80_BC,   Z80_Execute_Unimplemented},
    {0x0B, 1, "DEC BC",        Z80_BC,   0,        Z80_Execute_Unimplemented},
    {0x0C, 1, "INC C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x0D, 1, "DEC C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x0E, 2, "LD C,d8",       Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x0F, 1, "RRCA",          0,        0,        Z80_Execute_Unimplemented},
    {0x10, 2, "STOP",          0,        0,        Z80_Execute_Unimplemented},
    {0x11, 3, "LD DE,0x%04X\n",Z80_DE,   0,        Z80_Execute_LD_RR_NN},
    {0x12, 1, "LD (DE),A",     Z80_DE,   Z80_A,    Z80_Execute_Unimplemented},
    {0x13, 1, "INC DE",        Z80_DE,   0,        Z80_Execute_Unimplemented},
    {0x14, 1, "INC D",         Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x15, 1, "DEC D",         Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x16, 2, "LD D,d8",       Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x17, 1, "RLA",           0,        0,        Z80_Execute_Unimplemented},
    {0x18, 2, "JR r8",         0,        0,        Z80_Execute_Unimplemented},
    {0x19, 1, "ADD HL,DE",     Z80_HL,   Z80_DE,   Z80_Execute_Unimplemented},
    {0x1A, 1, "LD A,(DE)",     Z80_A,    Z80_DE,   Z80_Execute_Unimplemented},
    {0x1B, 1, "DEC DE",        Z80_DE,   0,        Z80_Execute_Unimplemented},
    {0x1C, 1, "INC E",         Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x1D, 1, "DEC E",         Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x1E, 2, "LD E,d8",       Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x1F, 1, "RRA",           0,        0,        Z80_Execute_Unimplemented},
    {0x20, 2, "JR NZ,r8",      0,        0,        Z80_Execute_Unimplemented},
    {0x21, 3, "LD HL,0x%04X\n",Z80_HL,   0,        Z80_Execute_LD_RR_NN},
    {0x22, 1, "LD (HL+),A",    Z80_HL,   Z80_A,    Z80_Execute_Unimplemented},
    {0x23, 1, "INC HL",        Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x24, 1, "INC H",         Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x25, 1, "DEC H",         Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x26, 2, "LD H,d8",       Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x27, 1, "DAA",           0,        0,        Z80_Execute_Unimplemented},
    {0x28, 2, "JR Z,r8",       0,        0,        Z80_Execute_Unimplemented},
    {0x29, 1, "ADD HL,HL",     Z80_HL,   Z80_HL,   Z80_Execute_Unimplemented},
    {0x2A, 1, "LD A,(HL+)",    Z80_A,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x2B, 1, "DEC HL",        Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x2C, 1, "INC L",         Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x2D, 1, "DEC L",         Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x2E, 2, "LD L,d8",       Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x2F, 1, "CPL",           0,        0,        Z80_Execute_Unimplemented},
    {0x30, 2, "JR NC,r8",      0,        0,        Z80_Execute_Unimplemented},
    {0x31, 3, "LD SP,0x%04X\n",Z80_SP,   0,        Z80_Execute_LD_RR_NN},
    {0x32, 1, "LD (HL-),A",    Z80_HL,   Z80_A,    Z80_Execute_Unimplemented},
    {0x33, 1, "INC SP",        Z80_SP,   0,        Z80_Execute_Unimplemented},
    {0x34, 1, "INC (HL)",      Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x35, 1, "DEC (HL)",      Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x36, 2, "LD (HL),d8",    Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x37, 1, "SCF",           0,        0,        Z80_Execute_Unimplemented},
    {0x38, 2, "JR C,r8",       0,        0,        Z80_Execute_Unimplemented},
    {0x39, 1, "ADD HL,SP",     Z80_HL,   Z80_SP,   Z80_Execute_Unimplemented},
    {0x3A, 1, "LD A,(HL-)",    Z80_A,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x3B, 1, "DEC SP",        Z80_SP,   0,        Z80_Execute_Unimplemented},
    {0x3C, 1, "INC A",         Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x3D, 1, "DEC A",         Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x3E, 2, "LD A,d8",       Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x3F, 1, "CCF",           0,        0,        Z80_Execute_Unimplemented},
    {0x40, 1, "LD B,B",        Z80_B,    Z80_B,    Z80_Execute_Unimplemented},
    {0x41, 1, "LD B,C",        Z80_B,    Z80_C,    Z80_Execute_Unimplemented},
    {0x42, 1, "LD B,D",        Z80_B,    Z80_D,    Z80_Execute_Unimplemented},
    {0x43, 1, "LD B,E",        Z80_B,    Z80_E,    Z80_Execute_Unimplemented},
    {0x44, 1, "LD B,H",        Z80_B,    Z80_H,    Z80_Execute_Unimplemented},
    {0x45, 1, "LD B,L",        Z80_B,    Z80_L,    Z80_Execute_Unimplemented},
    {0x46, 1, "LD B,(HL)",     Z80_B,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x47, 1, "LD B,A",        Z80_B,    Z80_A,    Z80_Execute_Unimplemented},
    {0x48, 1, "LD C,B",        Z80_C,    Z80_B,    Z80_Execute_Unimplemented},
    {0x49, 1, "LD C,C",        Z80_C,    Z80_C,    Z80_Execute_Unimplemented},
    {0x4A, 1, "LD C,D",        Z80_C,    Z80_D,    Z80_Execute_Unimplemented},
    {0x4B, 1, "LD C,E",        Z80_C,    Z80_E,    Z80_Execute_Unimplemented},
    {0x4C, 1, "LD C,H",        Z80_C,    Z80_H,    Z80_Execute_Unimplemented},
    {0x4D, 1, "LD C,L",        Z80_C,    Z80_L,    Z80_Execute_Unimplemented},
    {0x4E, 1, "LD C,(HL)",     Z80_C,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x4F, 1, "LD C,A",        Z80_C,    Z80_A,    Z80_Execute_Unimplemented},
    {0x50, 1, "LD D,B",        Z80_D,    Z80_B,    Z80_Execute_Unimplemented},
    {0x51, 1, "LD D,C",        Z80_D,    Z80_C,    Z80_Execute_Unimplemented},
    {0x52, 1, "LD D,D",        Z80_D,    Z80_D,    Z80_Execute_Unimplemented},
    {0x53, 1, "LD D,E",        Z80_D,    Z80_E,    Z80_Execute_Unimplemented},
    {0x54, 1, "LD D,H",        Z80_D,    Z80_H,    Z80_Execute_Unimplemented},
    {0x55, 1, "LD D,L",        Z80_D,    Z80_L,    Z80_Execute_Unimplemented},
    {0x56, 1, "LD D,(HL)",     Z80_D,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x57, 1, "LD D,A",        Z80_D,    Z80_A,    Z80_Execute_Unimplemented},
    {0x58, 1, "LD E,B",        Z80_E,    Z80_B,    Z80_Execute_Unimplemented},
    {0x59, 1, "LD E,C",        Z80_E,    Z80_C,    Z80_Execute_Unimplemented},
    {0x5A, 1, "LD E,D",        Z80_E,    Z80_D,    Z80_Execute_Unimplemented},
    {0x5B, 1, "LD E,E",        Z80_E,    Z80_E,    Z80_Execute_Unimplemented},
    {0x5C, 1, "LD E,H",        Z80_E,    Z80_H,    Z80_Execute_Unimplemented},
    {0x5D, 1, "LD E,L",        Z80_E,    Z80_L,    Z80_Execute_Unimplemented},
    {0x5E, 1, "LD E,(HL)",     Z80_E,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x5F, 1, "LD E,A",        Z80_E,    Z80_A,    Z80_Execute_Unimplemented},
    {0x60, 1, "LD H,B",        Z80_H,    Z80_B,    Z80_Execute_Unimplemented},
    {0x61, 1, "LD H,C",        Z80_H,    Z80_C,    Z80_Execute_Unimplemented},
    {0x62, 1, "LD H,D",        Z80_H,    Z80_D,    Z80_Execute_Unimplemented},
    {0x63, 1, "LD H,E",        Z80_H,    Z80_E,    Z80_Execute_Unimplemented},
    {0x64, 1, "LD H,H",        Z80_H,    Z80_H,    Z80_Execute_Unimplemented},
    {0x65, 1, "LD H,L",        Z80_H,    Z80_L,    Z80_Execute_Unimplemented},
    {0x66, 1, "LD H,(HL)",     Z80_H,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x67, 1, "LD H,A",        Z80_H,    Z80_A,    Z80_Execute_Unimplemented},
    {0x68, 1, "LD L,B",        Z80_L,    Z80_B,    Z80_Execute_Unimplemented},
    {0x69, 1, "LD L,C",        Z80_L,    Z80_C,    Z80_Execute_Unimplemented},
    {0x6A, 1, "LD L,D",        Z80_L,    Z80_D,    Z80_Execute_Unimplemented},
    {0x6B, 1, "LD L,E",        Z80_L,    Z80_E,    Z80_Execute_Unimplemented},
    {0x6C, 1, "LD L,H",        Z80_L,    Z80_H,    Z80_Execute_Unimplemented},
    {0x6D, 1, "LD L,L",        Z80_L,    Z80_L,    Z80_Execute_Unimplemented},
    {0x6E, 1, "LD L,(HL)",     Z80_L,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x6F, 1, "LD L,A",        Z80_L,    Z80_A,    Z80_Execute_Unimplemented},
    {0x70, 1, "LD (HL),B",     Z80_HL,   Z80_B,    Z80_Execute_Unimplemented},
    {0x71, 1, "LD (HL),C",     Z80_HL,   Z80_C,    Z80_Execute_Unimplemented},
    {0x72, 1, "LD (HL),D",     Z80_HL,   Z80_D,    Z80_Execute_Unimplemented},
    {0x73, 1, "LD (HL),E",     Z80_HL,   Z80_E,    Z80_Execute_Unimplemented},
    {0x74, 1, "LD (HL),H",     Z80_HL,   Z80_H,    Z80_Execute_Unimplemented},
    {0x75, 1, "LD (HL),L",     Z80_HL,   Z80_L,    Z80_Execute_Unimplemented},
    {0x76, 1, "HALT",          0,        0,        Z80_Execute_Unimplemented},
    {0x77, 1, "LD (HL),A",     Z80_HL,   Z80_A,    Z80_Execute_Unimplemented},
    {0x78, 1, "LD A,B",        Z80_A,    Z80_B,    Z80_Execute_Unimplemented},
    {0x79, 1, "LD A,C",        Z80_A,    Z80_C,    Z80_Execute_Unimplemented},
    {0x7A, 1, "LD A,D",        Z80_A,    Z80_D,    Z80_Execute_Unimplemented},
    {0x7B, 1, "LD A,E",        Z80_A,    Z80_E,    Z80_Execute_Unimplemented},
    {0x7C, 1, "LD A,H",        Z80_A,    Z80_H,    Z80_Execute_Unimplemented},
    {0x7D, 1, "LD A,L",        Z80_A,    Z80_L,    Z80_Execute_Unimplemented},
    {0x7E, 1, "LD A,(HL)",     Z80_A,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x7F, 1, "LD A,A",        Z80_A,    Z80_A,    Z80_Execute_Unimplemented},
    {0x80, 1, "ADD A,B",       Z80_A,    Z80_B,    Z80_Execute_Unimplemented},
    {0x81, 1, "ADD A,C",       Z80_A,    Z80_C,    Z80_Execute_Unimplemented},
    {0x82, 1, "ADD A,D",       Z80_A,    Z80_D,    Z80_Execute_Unimplemented},
    {0x83, 1, "ADD A,E",       Z80_A,    Z80_E,    Z80_Execute_Unimplemented},
    {0x84, 1, "ADD A,H",       Z80_A,    Z80_H,    Z80_Execute_Unimplemented},
    {0x85, 1, "ADD A,L",       Z80_A,    Z80_L,    Z80_Execute_Unimplemented},
    {0x86, 1, "ADD A,(HL)",    Z80_A,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x87, 1, "ADD A,A",       Z80_A,    Z80_A,    Z80_Execute_Unimplemented},
    {0x88, 1, "ADC A,B",       Z80_A,    Z80_B,    Z80_Execute_Unimplemented},
    {0x89, 1, "ADC A,C",       Z80_A,    Z80_C,    Z80_Execute_Unimplemented},
    {0x8A, 1, "ADC A,D",       Z80_A,    Z80_D,    Z80_Execute_Unimplemented},
    {0x8B, 1, "ADC A,E",       Z80_A,    Z80_E,    Z80_Execute_Unimplemented},
    {0x8C, 1, "ADC A,H",       Z80_A,    Z80_H,    Z80_Execute_Unimplemented},
    {0x8D, 1, "ADC A,L",       Z80_A,    Z80_L,    Z80_Execute_Unimplemented},
    {0x8E, 1, "ADC A,(HL)",    Z80_A,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x8F, 1, "ADC A,A",       Z80_A,    Z80_A,    Z80_Execute_Unimplemented},
    {0x90, 1, "SUB B",         Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x91, 1, "SUB C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x92, 1, "SUB D",         Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x93, 1, "SUB E",         Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x94, 1, "SUB H",         Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x95, 1, "SUB L",         Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x96, 1, "SUB (HL)",      Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x97, 1, "SUB A",         Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x98, 1, "SBC A,B",       Z80_A,    Z80_B,    Z80_Execute_Unimplemented},
    {0x99, 1, "SBC A,C",       Z80_A,    Z80_C,    Z80_Execute_Unimplemented},
    {0x9A, 1, "SBC A,D",       Z80_A,    Z80_D,    Z80_Execute_Unimplemented},
    {0x9B, 1, "SBC A,E",       Z80_A,    Z80_E,    Z80_Execute_Unimplemented},
    {0x9C, 1, "SBC A,H",       Z80_A,    Z80_H,    Z80_Execute_Unimplemented},
    {0x9D, 1, "SBC A,L",       Z80_A,    Z80_L,    Z80_Execute_Unimplemented},
    {0x9E, 1, "SBC A,(HL)",    Z80_A,    Z80_HL,   Z80_Execute_Unimplemented},
    {0x9F, 1, "SBC A,A",       Z80_A,    Z80_A,    Z80_Execute_Unimplemented},
    {0xA0, 1, "AND B",         Z80_B,    0,        Z80_Execute_Unimplemented},
    {0xA1, 1, "AND C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0xA2, 1, "AND D",         Z80_D,    0,        Z80_Execute_Unimplemented},
    {0xA3, 1, "AND E",         Z80_E,    0,        Z80_Execute_Unimplemented},
    {0xA4, 1, "AND H",         Z80_H,    0,        Z80_Execute_Unimplemented},
    {0xA5, 1, "AND L",         Z80_L,    0,        Z80_Execute_Unimplemented},
    {0xA6, 1, "AND (HL)",      Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0xA7, 1, "AND A",         Z80_A,    0,        Z80_Execute_Unimplemented},
    {0xA8, 1, "XOR B",         Z80_B,    0,        Z80_Execute_Unimplemented},
    {0xA9, 1, "XOR C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0xAA, 1, "XOR D",         Z80_D,    0,        Z80_Execute_Unimplemented},
    {0xAB, 1, "XOR E",         Z80_E,    0,        Z80_Execute_Unimplemented},
    {0xAC, 1, "XOR H",         Z80_H,    0,        Z80_Execute_Unimplemented},
    {0xAD, 1, "XOR L",         Z80_L,    0,        Z80_Execute_Unimplemented},
    {0xAE, 1, "XOR (HL)",      Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0xAF, 1, "XOR A",         Z80_A,    0,        Z80_Execute_Unimplemented},
    {0xB0, 1, "OR B",          Z80_B,    0,        Z80_Execute_Unimplemented},
    {0xB1, 1, "OR C",          Z80_C,    0,        Z80_Execute_Unimplemented},
    {0xB2, 1, "OR D",          Z80_D,    0,        Z80_Execute_Unimplemented},
    {0xB3, 1, "OR E",          Z80_E,    0,        Z80_Execute_Unimplemented},
    {0xB4, 1, "OR H",          Z80_H,    0,        Z80_Execute_Unimplemented},
    {0xB5, 1, "OR L",          Z80_L,    0,        Z80_Execute_Unimplemented},
    {0xB6, 1, "OR (HL)",       Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0xB7, 1, "OR A",          Z80_A,    0,        Z80_Execute_Unimplemented},
    {0xB8, 1, "CP B",          Z80_B,    0,        Z80_Execute_Unimplemented},
    {0xB9, 1, "CP C",          Z80_C,    0,        Z80_Execute_Unimplemented},
    {0xBA, 1, "CP D",          Z80_D,    0,        Z80_Execute_Unimplemented},
    {0xBB, 1, "CP E",          Z80_E,    0,        Z80_Execute_Unimplemented},
    {0xBC, 1, "CP H",          Z80_H,    0,        Z80_Execute_Unimplemented},
    {0xBD, 1, "CP L",          Z80_L,    0,        Z80_Execute_Unimplemented},
    {0xBE, 1, "CP (HL)",       Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0xBF, 1, "CP A",          Z80_A,    0,        Z80_Execute_Unimplemented},
    {0xC0, 1, "RET NZ",        0,        0,        Z80_Execute_Unimplemented},
    {0xC1, 1, "POP BC",        Z80_BC,   0,        Z80_Execute_Unimplemented},
    {0xC2, 3, "JP NZ,a16",     0,        0,        Z80_Execute_Unimplemented},
    {0xC3, 3, "JP a16",        0,        0,        Z80_Execute_Unimplemented},
    {0xC4, 3, "CALL NZ,a16",   0,        0,        Z80_Execute_Unimplemented},
    {0xC5, 1, "PUSH BC",       Z80_BC,   0,        Z80_Execute_Unimplemented},
    {0xC6, 2, "ADD A,d8",      Z80_A,    0,        Z80_Execute_Unimplemented},
    {0xC7, 1, "RST 00H",       0,        0,        Z80_Execute_Unimplemented},
    {0xC8, 1, "RET Z",         0,        0,        Z80_Execute_Unimplemented},
    {0xC9, 1, "RET",           0,        0,        Z80_Execute_Unimplemented},
    {0xCA, 3, "JP Z,a16",      0,        0,        Z80_Execute_Unimplemented},
    {0xCB, 1, "PREFIX CB",     0,        0,        Z80_Execute_Unimplemented},
    {0xCC, 3, "CALL Z,a16",    0,        0,        Z80_Execute_Unimplemented},
    {0xCD, 3, "CALL a16",      0,        0,        Z80_Execute_Unimplemented},
    {0xCE, 2, "ADC A,d8",      Z80_A,    0,        Z80_Execute_Unimplemented},
    {0xCF, 1, "RST 08H",       0,        0,        Z80_Execute_Unimplemented},
    {0xD0, 1, "RET NC",        0,        0,        Z80_Execute_Unimplemented},
    {0xD1, 1, "POP DE",        Z80_DE,   0,        Z80_Execute_Unimplemented},
    {0xD2, 3, "JP NC,a16",     0,        0,        Z80_Execute_Unimplemented},
    {0xD3, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xD4, 3, "CALL NC,a16",   0,        0,        Z80_Execute_Unimplemented},
    {0xD5, 1, "PUSH DE",       Z80_DE,   0,        Z80_Execute_Unimplemented},
    {0xD6, 2, "SUB d8",        0,        0,        Z80_Execute_Unimplemented},
    {0xD7, 1, "RST 10H",       0,        0,        Z80_Execute_Unimplemented},
    {0xD8, 1, "RET C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0xD9, 1, "RETI",          0,        0,        Z80_Execute_Unimplemented},
    {0xDA, 3, "JP C,a16",      Z80_C,    0,        Z80_Execute_Unimplemented},
    {0xDB, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xDC, 3, "CALL C,a16",    Z80_C,    0,        Z80_Execute_Unimplemented},
    {0xDD, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xDE, 2, "SBC A,d8",      Z80_A,    0,        Z80_Execute_Unimplemented},
    {0xDF, 1, "RST 18H",       0,        0,        Z80_Execute_Unimplemented},
    {0xE0, 2, "LDH (a8),A",    0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xE1, 1, "POP HL",        Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0xE2, 2, "LD (C),A",      Z80_C,    Z80_A,    Z80_Execute_Unimplemented},
    {0xE3, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xE4, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xE5, 1, "PUSH HL",       Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0xE6, 2, "AND d8",        0,        0,        Z80_Execute_Unimplemented},
    {0xE7, 1, "RST 20H",       0,        0,        Z80_Execute_Unimplemented},
    {0xE8, 2, "ADD SP,r8",     Z80_SP,   0,        Z80_Execute_Unimplemented},
    {0xE9, 1, "JP (HL)",       Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0xEA, 3, "LD (a16),A",    0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xEB, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xEC, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xED, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xEE, 2, "XOR d8",        0,        0,        Z80_Execute_Unimplemented},
    {0xEF, 1, "RST 28H",       0,        0,        Z80_Execute_Unimplemented},
    {0xF0, 2, "LDH A,(a8)",    Z80_A,    0,        Z80_Execute_Unimplemented},
    {0xF1, 1, "POP AF",        Z80_AF,   0,        Z80_Execute_Unimplemented},
    {0xF2, 2, "LD A,(C)",      Z80_A,    Z80_C,    Z80_Execute_Unimplemented},
    {0xF3, 1, "DI",            0,        0,        Z80_Execute_Unimplemented},
    {0xF4, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xF5, 1, "PUSH AF",       Z80_AF,   0,        Z80_Execute_Unimplemented},
    {0xF6, 2, "OR d8",         0,        0,        Z80_Execute_Unimplemented},
    {0xF7, 1, "RST 30H",       0,        0,        Z80_Execute_Unimplemented},
    {0xF8, 2, "LD HL,SP+r8",   Z80_HL,   Z80_SP,   Z80_Execute_Unimplemented},
    {0xF9, 1, "LD SP,HL",      Z80_SP,   Z80_HL,   Z80_Execute_Unimplemented},
    {0xFA, 3, "LD A,(a16)",    Z80_A,    0,        Z80_Execute_Unimplemented},
    {0xFB, 1, "EI",            0,        0,        Z80_Execute_Unimplemented},
    {0xFC, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xFD, 1, "-",             0,        0,        Z80_Execute_Unimplemented},
    {0xFE, 2, "CP d8",         0,        0,        Z80_Execute_Unimplemented},
    {0xFF, 1, "RST 38H",       0,        0,        Z80_Execute_Unimplemented}
};

static Z80_OpCode_t const Z80_OpCode_Prefix[] =
{
    {0x00, 2, "RLC B",         Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x01, 2, "RLC C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x02, 2, "RLC D",         Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x03, 2, "RLC E",         Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x04, 2, "RLC H",         Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x05, 2, "RLC L",         Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x06, 2, "RLC (HL)",      Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x07, 2, "RLC A",         Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x08, 2, "RRC B",         Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x09, 2, "RRC C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x0A, 2, "RRC D",         Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x0B, 2, "RRC E",         Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x0C, 2, "RRC H",         Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x0D, 2, "RRC L",         Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x0E, 2, "RRC (HL)",      Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x0F, 2, "RRC A",         Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x10, 2, "RL B",          Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x11, 2, "RL C",          Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x12, 2, "RL D",          Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x13, 2, "RL E",          Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x14, 2, "RL H",          Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x15, 2, "RL L",          Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x16, 2, "RL (HL)",       Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x17, 2, "RL A",          Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x18, 2, "RR B",          Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x19, 2, "RR C",          Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x1A, 2, "RR D",          Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x1B, 2, "RR E",          Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x1C, 2, "RR H",          Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x1D, 2, "RR L",          Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x1E, 2, "RR (HL)",       Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x1F, 2, "RR A",          Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x20, 2, "SLA B",         Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x21, 2, "SLA C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x22, 2, "SLA D",         Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x23, 2, "SLA E",         Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x24, 2, "SLA H",         Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x25, 2, "SLA L",         Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x26, 2, "SLA (HL)",      Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x27, 2, "SLA A",         Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x28, 2, "SRA B",         Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x29, 2, "SRA C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x2A, 2, "SRA D",         Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x2B, 2, "SRA E",         Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x2C, 2, "SRA H",         Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x2D, 2, "SRA L",         Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x2E, 2, "SRA (HL)",      Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x2F, 2, "SRA A",         Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x30, 2, "SWAP B",        Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x31, 2, "SWAP C",        Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x32, 2, "SWAP D",        Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x33, 2, "SWAP E",        Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x34, 2, "SWAP H",        Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x35, 2, "SWAP L",        Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x36, 2, "SWAP (HL)",     Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x37, 2, "SWAP A",        Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x38, 2, "SRL B",         Z80_B,    0,        Z80_Execute_Unimplemented},
    {0x39, 2, "SRL C",         Z80_C,    0,        Z80_Execute_Unimplemented},
    {0x3A, 2, "SRL D",         Z80_D,    0,        Z80_Execute_Unimplemented},
    {0x3B, 2, "SRL E",         Z80_E,    0,        Z80_Execute_Unimplemented},
    {0x3C, 2, "SRL H",         Z80_H,    0,        Z80_Execute_Unimplemented},
    {0x3D, 2, "SRL L",         Z80_L,    0,        Z80_Execute_Unimplemented},
    {0x3E, 2, "SRL (HL)",      Z80_HL,   0,        Z80_Execute_Unimplemented},
    {0x3F, 2, "SRL A",         Z80_A,    0,        Z80_Execute_Unimplemented},
    {0x40, 2, "BIT 0,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x41, 2, "BIT 0,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x42, 2, "BIT 0,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x43, 2, "BIT 0,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x44, 2, "BIT 0,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x45, 2, "BIT 0,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x46, 2, "BIT 0,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x47, 2, "BIT 0,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x48, 2, "BIT 1,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x49, 2, "BIT 1,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x4A, 2, "BIT 1,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x4B, 2, "BIT 1,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x4C, 2, "BIT 1,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x4D, 2, "BIT 1,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x4E, 2, "BIT 1,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x4F, 2, "BIT 1,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x50, 2, "BIT 2,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x51, 2, "BIT 2,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x52, 2, "BIT 2,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x53, 2, "BIT 2,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x54, 2, "BIT 2,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x55, 2, "BIT 2,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x56, 2, "BIT 2,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x57, 2, "BIT 2,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x58, 2, "BIT 3,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x59, 2, "BIT 3,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x5A, 2, "BIT 3,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x5B, 2, "BIT 3,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x5C, 2, "BIT 3,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x5D, 2, "BIT 3,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x5E, 2, "BIT 3,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x5F, 2, "BIT 3,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x60, 2, "BIT 4,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x61, 2, "BIT 4,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x62, 2, "BIT 4,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x63, 2, "BIT 4,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x64, 2, "BIT 4,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x65, 2, "BIT 4,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x66, 2, "BIT 4,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x67, 2, "BIT 4,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x68, 2, "BIT 5,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x69, 2, "BIT 5,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x6A, 2, "BIT 5,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x6B, 2, "BIT 5,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x6C, 2, "BIT 5,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x6D, 2, "BIT 5,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x6E, 2, "BIT 5,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x6F, 2, "BIT 5,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x70, 2, "BIT 6,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x71, 2, "BIT 6,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x72, 2, "BIT 6,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x73, 2, "BIT 6,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x74, 2, "BIT 6,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x75, 2, "BIT 6,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x76, 2, "BIT 6,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x77, 2, "BIT 6,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x78, 2, "BIT 7,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x79, 2, "BIT 7,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x7A, 2, "BIT 7,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x7B, 2, "BIT 7,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x7C, 2, "BIT 7,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x7D, 2, "BIT 7,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x7E, 2, "BIT 7,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x7F, 2, "BIT 7,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x80, 2, "RES 0,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x81, 2, "RES 0,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x82, 2, "RES 0,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x83, 2, "RES 0,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x84, 2, "RES 0,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x85, 2, "RES 0,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x86, 2, "RES 0,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x87, 2, "RES 0,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x88, 2, "RES 1,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x89, 2, "RES 1,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x8A, 2, "RES 1,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x8B, 2, "RES 1,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x8C, 2, "RES 1,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x8D, 2, "RES 1,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x8E, 2, "RES 1,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x8F, 2, "RES 1,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x90, 2, "RES 2,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x91, 2, "RES 2,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x92, 2, "RES 2,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x93, 2, "RES 2,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x94, 2, "RES 2,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x95, 2, "RES 2,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x96, 2, "RES 2,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x97, 2, "RES 2,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0x98, 2, "RES 3,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0x99, 2, "RES 3,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0x9A, 2, "RES 3,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0x9B, 2, "RES 3,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0x9C, 2, "RES 3,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0x9D, 2, "RES 3,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0x9E, 2, "RES 3,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0x9F, 2, "RES 3,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xA0, 2, "RES 4,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xA1, 2, "RES 4,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xA2, 2, "RES 4,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xA3, 2, "RES 4,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xA4, 2, "RES 4,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xA5, 2, "RES 4,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xA6, 2, "RES 4,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xA7, 2, "RES 4,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xA8, 2, "RES 5,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xA9, 2, "RES 5,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xAA, 2, "RES 5,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xAB, 2, "RES 5,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xAC, 2, "RES 5,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xAD, 2, "RES 5,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xAE, 2, "RES 5,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xAF, 2, "RES 5,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xB0, 2, "RES 6,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xB1, 2, "RES 6,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xB2, 2, "RES 6,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xB3, 2, "RES 6,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xB4, 2, "RES 6,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xB5, 2, "RES 6,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xB6, 2, "RES 6,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xB7, 2, "RES 6,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xB8, 2, "RES 7,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xB9, 2, "RES 7,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xBA, 2, "RES 7,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xBB, 2, "RES 7,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xBC, 2, "RES 7,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xBD, 2, "RES 7,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xBE, 2, "RES 7,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xBF, 2, "RES 7,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xC0, 2, "SET 0,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xC1, 2, "SET 0,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xC2, 2, "SET 0,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xC3, 2, "SET 0,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xC4, 2, "SET 0,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xC5, 2, "SET 0,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xC6, 2, "SET 0,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xC7, 2, "SET 0,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xC8, 2, "SET 1,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xC9, 2, "SET 1,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xCA, 2, "SET 1,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xCB, 2, "SET 1,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xCC, 2, "SET 1,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xCD, 2, "SET 1,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xCE, 2, "SET 1,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xCF, 2, "SET 1,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xD0, 2, "SET 2,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xD1, 2, "SET 2,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xD2, 2, "SET 2,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xD3, 2, "SET 2,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xD4, 2, "SET 2,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xD5, 2, "SET 2,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xD6, 2, "SET 2,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xD7, 2, "SET 2,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xD8, 2, "SET 3,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xD9, 2, "SET 3,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xDA, 2, "SET 3,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xDB, 2, "SET 3,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xDC, 2, "SET 3,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xDD, 2, "SET 3,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xDE, 2, "SET 3,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xDF, 2, "SET 3,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xE0, 2, "SET 4,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xE1, 2, "SET 4,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xE2, 2, "SET 4,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xE3, 2, "SET 4,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xE4, 2, "SET 4,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xE5, 2, "SET 4,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xE6, 2, "SET 4,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xE7, 2, "SET 4,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xE8, 2, "SET 5,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xE9, 2, "SET 5,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xEA, 2, "SET 5,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xEB, 2, "SET 5,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xEC, 2, "SET 5,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xED, 2, "SET 5,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xEE, 2, "SET 5,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xEF, 2, "SET 5,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xF0, 2, "SET 6,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xF1, 2, "SET 6,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xF2, 2, "SET 6,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xF3, 2, "SET 6,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xF4, 2, "SET 6,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xF5, 2, "SET 6,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xF6, 2, "SET 6,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xF7, 2, "SET 6,A",       0,        Z80_A,    Z80_Execute_Unimplemented},
    {0xF8, 2, "SET 7,B",       0,        Z80_B,    Z80_Execute_Unimplemented},
    {0xF9, 2, "SET 7,C",       0,        Z80_C,    Z80_Execute_Unimplemented},
    {0xFA, 2, "SET 7,D",       0,        Z80_D,    Z80_Execute_Unimplemented},
    {0xFB, 2, "SET 7,E",       0,        Z80_E,    Z80_Execute_Unimplemented},
    {0xFC, 2, "SET 7,H",       0,        Z80_H,    Z80_Execute_Unimplemented},
    {0xFD, 2, "SET 7,L",       0,        Z80_L,    Z80_Execute_Unimplemented},
    {0xFE, 2, "SET 7,(HL)",    0,        Z80_HL,   Z80_Execute_Unimplemented},
    {0xFF, 2, "SET 7,A",       0,        Z80_A,    Z80_Execute_Unimplemented}
};


/******************************************************/
/* Function                                           */
/******************************************************/

void Z80_Initialize(void)
{
    for(int i=0; i<Z80_REG_NUM; i++)
    {
        Z80_REG16(i)->UWord = 0;
    }
}

void Z80_Run(void)
{
    /* Get instruction */
    uint16_t const pc = Z80_REG16(Z80_PC)->UWord;
    uint8_t const data = Memory_Read(pc);
    Z80_OpCode_t const * opcode = &Z80_OpCode[data];

    /* Execute instruction */
    opcode->ExecuteCallback(opcode);
}

void Z80_Print(void)
{
    LOG_INFO("Z80 Register:\n");
    LOG_INFO("AF = 0x%04X\n", Z80_REG16(Z80_AF)->UWord);
    LOG_INFO("BC = 0x%04X\n", Z80_REG16(Z80_BC)->UWord);
    LOG_INFO("DE = 0x%04X\n", Z80_REG16(Z80_DE)->UWord);
    LOG_INFO("HL = 0x%04X\n", Z80_REG16(Z80_HL)->UWord);
    LOG_INFO("SP = 0x%04X\n", Z80_REG16(Z80_SP)->UWord);
    LOG_INFO("PC = 0x%04X\n", Z80_REG16(Z80_PC)->UWord);
}

/**
 * OpCode: XXXXXX
 * Size:X, Duration:X, ZNHC Flag:XXXX
 */
static int Z80_Execute_Unimplemented(Z80_OpCode_t const * const opcode)
{
    LOG_ERROR("Implemented Opcode 0x%02X: %s\n", opcode->Value, opcode->Name);
    assert(0);
    return 0;
}


/**
 * OpCode: LD RR,d16
 * Size:3, Duration:12, ZNHC Flag:----
 */
static int Z80_Execute_LD_RR_NN(Z80_OpCode_t const * const opcode)
{
    /* Get the opcde parameter */
    uint16_t const pc = Z80_REG16(Z80_PC)->UWord;
    uint8_t const data0 = Memory_Read(pc + 1);
    uint8_t const data1 = Memory_Read(pc + 2);

    LOG_DEBUG(opcode->Name, CONCAT(data0, data1));

    /* Execute the command */
    Z80_REG16(opcode->Param0)->Byte[0].UByte = data0;
    Z80_REG16(opcode->Param0)->Byte[1].UByte = data1;
    Z80_REG16(Z80_PC)->UWord += 3;

    return 12;
}


