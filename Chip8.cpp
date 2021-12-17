#include "Chip8.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <random>

const unsigned int FONTSET_SIZE = 80;            //There are 16 "sprites" at 5 char each
const unsigned int FONTSET_START_ADDRESS = 0x50; //Where in memory the data can be stored for the screen sprites
const unsigned int START_ADDRESS = 0x200;        //Where in memory the data can be stored

//the 5 byte representation of each "sptite"
uint8_t fontset[FONTSET_SIZE] =
    {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

//reads in a rom
void Chip8::LoadROM(char const *filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (file.is_open())
    {
        std::streampos size = file.tellg();
        char *buffer = new char[size];
        file.seekg(0, std::ios::beg);
        file.read(buffer, size);
        file.close();

        for (long i = 0; i < size; ++i)
        {
            memory[START_ADDRESS + i] = buffer[i];
        }

        delete[] buffer;
    }
}

Chip8::Chip8() : randGen(std::chrono::system_clock::now().time_since_epoch().count())
{
    // Initialize PC
    pc = START_ADDRESS;

    // Load fonts into memory
    for (unsigned int i = 0; i < FONTSET_SIZE; ++i)
    {
        memory[FONTSET_START_ADDRESS + i] = fontset[i];
    }

    // Initialize RNG
    randByte = std::uniform_int_distribution<uint8_t>(0, 255U);
}

//opcodes

//CLS: Clears the screen
void Chip8::OP_00E0()
{
    memset(video, 0, sizeof(video));
}

//RET: return from a subroutine, aka a method
void Chip8::OP_00EE()
{
    --sp;           //decrement the stack pointer
    pc = stack[sp]; //this address should have the instruction in memory where to go next
}

//JP: jump to location at address nnn
void Chip8::OP_1nnn()
{
    uint16_t address = opcode & 0x0FFFu; //"and" operation makes sure to grab the nnn of the opcode and sets it to the address

    pc = address; //the program counter is set to this location
}

//CALL: Calling a subroutine
void Chip8::OP_2nnn()
{
    uint16_t address = opcode & 0x0FFFu;

    stack[sp] = pc;
    ++sp;
    pc = address;
}

//SKIP(SE): Skip next instruction if Vx = kk
void Chip8::OP_3xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t byte = opcode & 0x00FFu;

    if (registers[Vx] == byte)
    {
        pc += 2; //skips the next instruction
    }
}

//SKIP(SNE): Skip next instruction if Vx != kk
void Chip8::OP_3xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t byte = opcode & 0x00FFu;

    if (registers[Vx] != byte)
    {
        pc += 2; //skips the next instruction
    }
}

//SKIP(SE Vx, Vy): skips if register[Vx] == register[Vy]
void Chip8::OP_5xy0()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t Vy = (opcode & 0x00F0u) >> 4u; //shift to 0x000D

    if (registers[Vx] == registers[Vy])
    {
        pc += 2; //skips the next instruction
    }
}

//LD: Load regiter Vx with byte of data
void Chip8::OP_6xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t byte = opcode & 0x00FFu;

    registers[Vx] = byte;
}

//ADD: register Vx and ADD byte kk
void Chip8::OP_7xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t byte = (opcode & 0x00FFu);

    registers[Vx] += byte;
}

//LD: Load registers[Vx] with register[Vy]
void Chip8::OP_8xy0()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    registers[Vx] = registers[Vy];
}

//OR: set vx = vx | vy 
void Chip8::OP_8xy1()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t Vy = (opcode & 0x00F0u) >> 4u; //shift to 0x000F

    registers[Vx] |= registers[Vy];
}

//AND: set Vx = Vx & Vy
void Chip8::OP_8xy2()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t Vy = (opcode & 0x00F0u) >> 4u; //shift to 0x000F

    registers[Vx] &= registers[Vy];
}

//XOR; set Vx = Vx ^ Vy
void Chip8::OP_8xy3()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t Vy = (opcode & 0x00F0u) >> 4u; //shift to 0x000F

    registers[Vx] ^= registers[Vy];
}

//ADD: vx += vy, with carry flag
void Chip8::OP_8xy4()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t Vy = (opcode & 0x00F0u) >> 4u; //shift to 0x000F

    uint16_t sum = registers[Vx] + registers[Vy];

    //overflow
    if(sum > 255u){
        registers[0xF] = 1; //the register specifically made for flag
    }
    else //no overflow
    {
        registers[0xF] = 0;
    }

    registers[Vx] = sum & 0xFFu; //we only want the last byte
}

//SUB: Vx = Vx - Vy
void Chip8::OP_8xy5()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t Vy = (opcode & 0x00F0u) >> 4u; //shift to 0x000F

    if(registers[Vx] > registers[Vy])
    {
        registers[0xF] = 1; //wont go into negative
    }
    else //will go to negative
    {
        registers[0xF] = 0;
    }

    registers[Vx] -= registers[Vy];
}

///SHR: right shifts Vx
void Chip8::OP_8xy6()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    
    registers[0xF] = (registers[Vx] & 0x1u);

    registers[Vx] >>= 1;
}

//SUBN: xv = xy - vx
void Chip8::OP_8xy7()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    uint8_t Vy = (opcode & 0x00F0u) >> 4u; //shift to 0x000F

    if (registers[Vy] > registers[Vx])
	{
		registers[0xF] = 1;
	}
	else
	{
		registers[0xF] = 0;
	}

	registers[Vx] = registers[Vy] - registers[Vx];
}

///SHL: right shifts Vx
void Chip8::OP_8xyE()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; //shift to 0x000F
    
    //MSB to VF
    registers[0xF] = (registers[Vx] & 0x80u) >> 7u;

    registers[Vx] <<= 1;
}

//SKIP: SNE, if Vx != Vy
void Chip8::OP_9xy0()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;

	if (registers[Vx] != registers[Vy])
	{
		pc += 2;
	}
}

//LOAD: load the index with address
void Chip8::OP_Annn()
{
	uint16_t address = opcode & 0x0FFFu;

	index = address;
}

//JUMP: jump to location to nnn + V0
void Chip8::OP_Bnnn()
{
	uint16_t address = opcode & 0x0FFFu;

	pc = registers[0] + address;
}

//SET: vx = random byte and kk
void Chip8::OP_Cxkk()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t byte = opcode & 0x00FFu;

	registers[Vx] = randByte(randGen) & byte;
}


//draw to the screen, need to look into it more to understand what exactly is happening here 
//TODO :
/*
We iterate over the sprite, row by row and column by column. We know there are eight columns because a sprite is guaranteed to be eight pixels wide.

If a sprite pixel is on then there may be a collision with what’s already being displayed, so we check if our screen pixel in the same location is set. 
If so we must set the VF register to express collision.

Then we can just XOR the screen pixel with 0xFFFFFFFF to essentially XOR it with the sprite pixel (which we now know is on). We can’t XOR 
directly because the sprite pixel is either 1 or 0 while our video pixel is either 0x00000000 or 0xFFFFFFFF.
*/
void Chip8::OP_Dxyn()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;
	uint8_t height = opcode & 0x000Fu;

	// Wrap if going beyond screen boundaries
	uint8_t xPos = registers[Vx] % VIDEO_WIDTH;
	uint8_t yPos = registers[Vy] % VIDEO_HEIGHT;

	registers[0xF] = 0;

	for (unsigned int row = 0; row < height; ++row)
	{
		uint8_t spriteByte = memory[index + row];

		for (unsigned int col = 0; col < 8; ++col)
		{
			uint8_t spritePixel = spriteByte & (0x80u >> col);
			uint32_t* screenPixel = &video[(yPos + row) * VIDEO_WIDTH + (xPos + col)];

			// Sprite pixel is on
			if (spritePixel)
			{
				// Screen pixel also on - collision
				if (*screenPixel == 0xFFFFFFFF)
				{
					registers[0xF] = 1;
				}

				// Effectively XOR with the sprite pixel
				*screenPixel ^= 0xFFFFFFFF;
			}
		}
	}
}

//SKIP: if a key value has been pressed then skip
void Chip8::OP_Ex9E()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	uint8_t key = registers[Vx];

	if (keypad[key])
	{
		pc += 2;
	}
}

//SKIP: skip if the key value has not been pressed
void Chip8::OP_ExA1()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	uint8_t key = registers[Vx];

	if (!keypad[key])
	{
		pc += 2;
	}
}

//SET: set a register with a delay timer value
void Chip8::OP_Fx07()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	registers[Vx] = delayTimer;
}

//wait for a key press and store that keypress, pc -= 2 is the same as stalling out
void Chip8::OP_Fx0A()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	if (keypad[0])
	{
		registers[Vx] = 0;
	}
	else if (keypad[1])
	{
		registers[Vx] = 1;
	}
	else if (keypad[2])
	{
		registers[Vx] = 2;
	}
	else if (keypad[3])
	{
		registers[Vx] = 3;
	}
	else if (keypad[4])
	{
		registers[Vx] = 4;
	}
	else if (keypad[5])
	{
		registers[Vx] = 5;
	}
	else if (keypad[6])
	{
		registers[Vx] = 6;
	}
	else if (keypad[7])
	{
		registers[Vx] = 7;
	}
	else if (keypad[8])
	{
		registers[Vx] = 8;
	}
	else if (keypad[9])
	{
		registers[Vx] = 9;
	}
	else if (keypad[10])
	{
		registers[Vx] = 10;
	}
	else if (keypad[11])
	{
		registers[Vx] = 11;
	}
	else if (keypad[12])
	{
		registers[Vx] = 12;
	}
	else if (keypad[13])
	{
		registers[Vx] = 13;
	}
	else if (keypad[14])
	{
		registers[Vx] = 14;
	}
	else if (keypad[15])
	{
		registers[Vx] = 15;
	}
	else
	{
		pc -= 2;
	}
}

//SET: Sets the delay timer from whatever register is choosen
void Chip8::OP_Fx15()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	delayTimer = registers[Vx];
}

//SET: Sets the sound timer from whatever register is choosen
void Chip8::OP_Fx18()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	soundTimer = registers[Vx];
}

//ADD: The index is added from whatever register is choosen
void Chip8::OP_Fx1E()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	index += registers[Vx];
}

//SET: sets the index to the location of the sprite 
void Chip8::OP_Fx29()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t digit = registers[Vx];

	index = FONTSET_START_ADDRESS + (5 * digit);
}

//Store the register in BCD format, hundreds place is stored at memory location I, tens place I+1, ones-place I+2
void Chip8::OP_Fx33()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t value = registers[Vx];

	// Ones-place
	memory[index + 2] = value % 10;
	value /= 10;

	// Tens-place
	memory[index + 1] = value % 10;
	value /= 10;

	// Hundreds-place
	memory[index] = value % 10;
}

//Store registers from v0 to vx at memory location I
void Chip8::OP_Fx55()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	for (uint8_t i = 0; i <= Vx; ++i)
	{
		memory[index + i] = registers[i];
	}
}

//Reads from memory using the memory loacted from v0 to vx
void Chip8::OP_Fx65()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	for (uint8_t i = 0; i <= Vx; ++i)
	{
		registers[i] = memory[index + i];
	}
}

