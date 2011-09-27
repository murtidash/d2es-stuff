﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using CharacterEditor;

namespace ItemPacketParserTest
{
	class Program
	{
		static void Main(string[] args)
		{
			ItemDefs.LoadItemDefs();

			//9C 04 17 10 B7 01 00 00 10 00 82 00 65 00 00 22 F6 86 07 82 80 F0 1F 
			//box Horadric Cube
			//byte[] packet = { 0x9C, 0x04, 0x1F, 0x01, 0xB8, 0x01, 0x00, 0x00, 0x10, 0x08, 0x80, 0x00, 0x65, 0x00, 0xC0, 0x72, 0x46, 0x87, 0x06, 0x02, 0x96, 0xD8, 0x86, 0x93, 0xBA, 0xE9, 0x18, 0xE8, 0x20, 0xFF, 0x01 };
			
			// Problem packet!
			byte[] packet = { 0x39, 0x00, 0x1C, 0x08, 0x2E, 0x98, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00, 0xA6, 0x31, 0x73, 0x41, 0x6C, 0x40, 0xF3, 0x77, 0x30, 0x20, 0x73, 0xC6, 0xEE, 0x6C, 0x93, 0x86, 0x87, 0xC2, 0x98, 0xB1, 0x07, 0x72, 0x07, 0xA5, 0x03, 0xDA, 0x81, 0xEB, 0x40, 0xF5, 0xD3, 0x66, 0x59, 0xB2, 0xA2, 0x63, 0x86, 0x50, 0xE8, 0x50, 0x17, 0xF8, 0xFF, 0x80 };
			
			Item parsedItem = new Item(packet);

			Console.WriteLine(parsedItem);
			foreach (var item in parsedItem.Properties)
			{
				Console.WriteLine("\t" + item);
			}


		}
	}
}