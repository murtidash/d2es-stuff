﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net.Sockets;
using System.IO;
using System.Net;

namespace IdleClient.Chat
{
	/// <summary>
	/// Arguments for the realm server. 
	/// </summary>
	public class RealmServerArgs : EventArgs
	{
		internal RealmServerArgs(LogonRealmExIn packet)
		{
			MCPCookie = packet.MCPCookie;
			MCPStatus = packet.MCPStatus;
			Ip = packet.Ip;
			Port = packet.Port;
			UniqueName = packet.UniqueName;

			MCPChunk1 = (byte[])packet.MCPChunk1.Clone();
			MCPChunk2 = (byte[])packet.MCPChunk2.Clone();
		}

		public uint MCPCookie { get; set; }
		public uint MCPStatus { get; set; }
		public byte[] MCPChunk1 { get; set; }
		public IPAddress Ip { get; set; }
		public short Port { get; set; }
		public byte[] MCPChunk2 { get; set; }
		public string UniqueName { get; set; }
	}

	class ChatServer
	{
		/// <summary> Event queue for all listeners interested in OnDisconnect events. </summary>
		public event EventHandler OnDisconnect;

		/// <summary> 
		/// Event queue for all listeners interested in ReadyToConnectToGameServer events. There
		/// must exist atleast one listener to this event. 
		/// </summary>
		public event EventHandler<RealmServerArgs> ReadyToConnectToRealmServer;

		private bool isDisconnecting;
		private TcpClient client = new TcpClient();
		private Config settings;
		private uint clientToken = 0;
		private uint serverToken = 0;

		public ChatServer(Config settings)
		{
			this.settings = settings;
		}

		//C > S [0x50] SID_AUTH_INFO
		//S > C [0x25] SID_PING
		//S > C [0x50] SID_AUTH_INFO
		//C > S [0x51] SID_AUTH_CHECK
		//S > C [0x51] SID_AUTH_CHECK
		//C > S [0x3A] SID_LOGONRESPONSE2
		//S > C [0x3A] SID_LOGONRESPONSE2
		//C > S [0x0A] SID_ENTERCHAT		<--
		//S > C [0x0A] SID_ENTERCHAT
		// http://www.bnetdocs.org/old/content9d2c.html?Section=d&id=6
		public void Run()
		{
			Console.WriteLine("Connected to chat server " + settings.Address + ":" + settings.Port);

			try
			{
				client = new TcpClient(settings.Address, settings.Port);
			}
			catch (SocketException ex)
			{
				Console.WriteLine("Failed to connect to chat server: " + ex.Message);
				FireOnDisconnectEvent();
				return;
			}

			using (client)
			{
				// Used to store the unprocessed packet data from ReceivePacket
				byte[] buffer = new byte[0];

				// When connecting to the server we must specify the protocol to use.
				NetworkStream ns = client.GetStream();
				ns.WriteByte(0x01);

				// Must also send auth info to the server after the protocol
				SendPacket(new AuthInfoOut(settings.ClientVersion));

				while (client.Connected)
				{
					ChatServerPacket packet;

					try
					{
						packet = ReceivePacket(ref buffer);
					}
					catch (Exception ex)
					{
						if (!isDisconnecting)
						{
							Console.WriteLine("Failed to receive chat packet: " + ex.Message);
							Disconnect();
						}
						break;
					}

					if (settings.ShowPackets)
					{
						Console.WriteLine("S -> C: " + packet);
					}
					if (settings.ShowPacketData)
					{
						Console.WriteLine("Data: {0:X2} {1}", (byte)packet.Id, Util.GetStringOfBytes(packet.Data, 0, packet.Data.Length));
					}

					switch (packet.Id)
					{
						case ChatServerPacketType.PING:
							OnPing(packet);
							break;
						case ChatServerPacketType.AUTH_INFO:
							OnAuthInfo(packet);
							break;
						case ChatServerPacketType.AUTH_CHECK:
							OnAuthCheck(packet);
							break;
						case ChatServerPacketType.LOGONRESPONSE2:
							OnLogonResponse2(packet);
							break;
						case ChatServerPacketType.QUERYREALMS2:
							OnQueryRealms2(packet);
							break;
						case ChatServerPacketType.LOGONREALMEX:
							OnLogonRealmEx(packet);
							break;
						default:
							Console.WriteLine("Unhandled Chat server packet");
							Console.WriteLine(packet);
							break;
					}
				}
			}

			Console.WriteLine("Chat server: Disconnected");
			FireOnDisconnectEvent();
		}

		/// <summary>Handles the LogonResponse2 packet. Responds by sending a QueryRealms2 Packet </summary>
		/// <param name="packet">The packet.</param>
		private void OnLogonResponse2(ChatServerPacket packet)
		{
			LogonResponse2In fromServer = new LogonResponse2In(packet);
			Console.WriteLine(fromServer.ToString());

			if (!fromServer.IsSuccessful())
			{
				Disconnect();
				return;
			}

			SendPacket(ChatServerPacketType.QUERYREALMS2, new byte[0]);
		}

		/// <summary>Handles the QueryRealms2 packet. Responds by sending LogonRealmEx packet.</summary>
		/// <param name="packet">The packet.</param>
		private void OnQueryRealms2(ChatServerPacket packet)
		{
			QueryRealms2In fromServer = new QueryRealms2In(packet);
			Console.WriteLine(fromServer.ToString());

			if (fromServer.Count == 0)
			{
				Console.WriteLine("No realms available");
				Disconnect();
				return;
			}

			LogonRealmExOut toServer = new LogonRealmExOut(clientToken, serverToken, fromServer.Realms[0].Title);
			SendPacket(toServer);
		}

		/// <summary>Handles the LogonRealmEx packet. Signals manager to start up Realm Server handler</summary>
		/// <param name="packet">The packet.</param>
		private void OnLogonRealmEx(ChatServerPacket packet)
		{
			LogonRealmExIn fromServer = new LogonRealmExIn(packet);
			Console.WriteLine(fromServer.ToString());

			if (!fromServer.IsSuccessful())
			{
				Disconnect();
				return;
			}

			EventHandler<RealmServerArgs> tempHandler = ReadyToConnectToRealmServer;
			if (tempHandler != null)
			{
				tempHandler(this, new RealmServerArgs(fromServer));
			}
		}

		/// <summary>Handles the AuthCheck packet. Responds with LogonRequest2 packet</summary>
		/// <param name="packet">The packet.</param>
		private void OnAuthCheck(ChatServerPacket packet)
		{
			AuthCheckIn fromServer = new AuthCheckIn(packet);
			Console.WriteLine(fromServer.ToString());

			LogonRequest2Out toServer = new LogonRequest2Out(settings.Username, settings.password, clientToken, serverToken);
			SendPacket(toServer);
		}

		/// <summary>Handles the AuthInfo packet. Responds with AuthCheck packet</summary>
		/// <param name="packet">The packet.</param>
		private void OnAuthInfo(ChatServerPacket packet)
		{
			AuthInfoIn fromServer = new AuthInfoIn(packet);
			Console.WriteLine(fromServer.ToString());

			AuthCheckOut toServer = new AuthCheckOut(settings.Owner);

			serverToken = fromServer.ServerToken;
			clientToken = toServer.ClientToken;

			SendPacket(toServer);
		}

		/// <summary>Handles the Ping packet. Responds with the same packet</summary>
		/// <param name="packet">The packet.</param>
		private void OnPing(ChatServerPacket packet)
		{
			SendPacket(ChatServerPacketType.PING, packet.Data);
		}

		/// <summary>
		/// Sends a packet to the chat server.
		/// </summary>
		/// <param name="packet">The packet.</param>
		public void SendPacket(IOutPacket packet)
		{
			SendPacket((ChatServerPacketType)packet.Id, packet.GetBytes());
		}

		/// <summary>
		/// Sends a packet to the chat server.
		/// </summary>
		/// <param name="type">The type of packet to send.</param>
		/// <param name="data">The packet data (Not the packet header).</param>
		private void SendPacket(ChatServerPacketType type, byte[] data)
		{
			ChatServerPacket packet;

			packet.Magic = 0xff;
			packet.Length = (ushort)(data.Length + 4);
			packet.Id = type;
			packet.Data = data;

			if (settings.ShowPackets)
			{
				Console.WriteLine("C -> S: " + packet);
			}
			if (settings.ShowPacketData)
			{
				Console.WriteLine("Data: {0:X2} {1}", (byte)packet.Id, Util.GetStringOfBytes(packet.Data, 0, packet.Data.Length));
			}

			byte[] packetBytes = packet.GetBytes();
			try
			{
				client.GetStream().Write(packetBytes, 0, packetBytes.Length);
			}
			catch (Exception ex)
			{
				if (!isDisconnecting)
				{
					Console.WriteLine("Failed to send packet to chat server: " + ex.Message);
					Disconnect();
				}
				return;
			}
		}

		/// <summary>
		/// Receives a chat server packet. 
		/// </summary>
		/// <param name="buffer">
		/// [in,out] The buffer used to hold excess data retrieved from the server for use in future
		/// calls to ReceivePacket.
		/// </param>
		/// <returns>Chat server packet </returns>
		private ChatServerPacket ReceivePacket(ref byte[] buffer)
		{
			NetworkStream ns = client.GetStream();
			ChatServerPacket packet;
			bool needsMoreData = false;
			packet.Magic = 0xff;

			while (true)
			{
				if (buffer.Length == 0 || needsMoreData)
				{
					Util.Receive(ns, ref buffer);
					needsMoreData = false;
				}

				// All chat server packets begin with 0xff
				if (buffer[0] != 0xff)
				{
					throw new ApplicationException("Invalid data received");
				}

				// Needs enough data for header portion
				if (buffer.Length < 4)
				{
					needsMoreData = true;
					continue;
				}

				// Packet header contains enough bytes for packet header, read it
				BinaryReader br = new BinaryReader(new MemoryStream(buffer));
				packet.Magic = br.ReadByte();
				packet.Id = (ChatServerPacketType)br.ReadByte();
				packet.Length = br.ReadUInt16();

				// Now that we have the packet length, check to make sure we have 
				//   enough bytes in the buffer to read the entire packet.
				if (packet.Length > buffer.Length)
				{
					needsMoreData = true;
					continue;
				}

				packet.Data = br.ReadBytes(packet.Length - 4);
				break;
			}

			// Remove the processed packet data from buffer
			int newBufferLength = buffer.Length - packet.Length;
			Array.Copy(buffer, packet.Length, buffer, 0, newBufferLength);
			Array.Resize(ref buffer, newBufferLength);

			return packet;
		}

		/// <summary>
		/// Forcefully disconnects from the realm server.
		/// </summary>
		public void Disconnect()
		{
			if (client.Connected)
			{
				Console.WriteLine("Chat server: Disconnect requested");
				isDisconnecting = true;
				client.Close();
			}
		}

		/// <summary>
		/// Raises the on disconnect event. 
		/// </summary>
		private void FireOnDisconnectEvent()
		{
			EventHandler tempHandler = OnDisconnect;
			if (tempHandler != null)
			{
				tempHandler(this, new EventArgs());
			}
		}
	}

}