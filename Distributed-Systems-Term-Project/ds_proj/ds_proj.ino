#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_pthread.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "model.hpp"

#define FORMAT_LITTLEFS_IF_FAILED true

struct Data
{
	std::string state	 = "unknown";
	int			time_cnt = 0;

	Data( const std::string & _state, int _time_cnt ) :
		state { _state },
		time_cnt { _time_cnt }
	{
	}

	Data()							  = default;
	Data( const Data & )			  = default;
	Data( Data && )					  = default;
	Data & operator= ( const Data & ) = default;
	Data & operator= ( Data && )	  = default;
	~Data()							  = default;
};

std::mutex								info_mutex {};
std::unordered_map< std::string, Data > all_info {};
std::mt19937							gen( std::random_device {}() );
std::uniform_real_distribution< float > dis( -1.0f, 1.0f );
MyModel									model {};

/* WiFi network name and password */
// const char * ssid	  = "MSI Lappy";
// const char * password = "M$1ut$@v";
const char * ssid	  = "SMR_LAB";
const char * password = "smrl1991";

// IP address to send UDP data to.
// it can be ip address of the server or
// a network broadcast address
// here is broadcast address
const char * udpAddress = "10.5.20.255";
const int	 udpPort	= 10000;

const char * my_name	 = "esp3";
const char * all_names[] = { "esp1", "esp2", "esp3" };

// create UDP instance
WiFiUDP udp;

// Web server
WebServer server( 80 );

inline void print_row( const std::vector< float > & row )
{
	// also print comma in a pretty way
	Serial.print( "[ " );
	for ( size_t i = 0; i < row.size(); ++i )
	{
		Serial.printf( "% 2.5f", row[ i ] );
		if ( i < row.size() - 1 ) { Serial.print( ", " ); }
	}
	Serial.print( " ]" );
}

void clearFile( fs::FS & fs, const char * path )
{
	File file = fs.open( path, FILE_WRITE );
	file.close();
}

String readFile( fs::FS & fs, const char * path )
{
	File file = fs.open( path );
	if ( not file ) return "";

	String info = file.readString();

	file.close();
	return info;
}

void appendFile( fs::FS & fs, const char * path, const char * message )
{
	if ( not fs.exists( path ) )
	{
		File file = fs.open( path, FILE_WRITE );
		file.close();
	}

	File file = fs.open( path, FILE_APPEND );
	if ( not file ) return;

	file.println( message );
	file.close();
}

void writeFile( fs::FS & fs, const char * path, const char * message )
{
	File file = fs.open( path, FILE_WRITE );
	if ( not file ) return;

	file.println( message );
	file.close();
}

void setup()
{
	Serial.begin( 115200 );
	while ( not Serial ) { ; }

	if ( ! LittleFS.begin( FORMAT_LITTLEFS_IF_FAILED ) )
	{
		Serial.println( "LittleFS Mount Failed" );
		return;
	}

	Serial.printf( "\n\nMY NAME: %s\n\n", my_name );

	Serial.println( "Initializing model: " );
	model.init();
	Serial.println( "Model initialized!\n" );

	// Connect to the WiFi network
	WiFi.begin( ssid, password );
	Serial.println();

	// Wait for connection
	while ( WiFi.status() != WL_CONNECTED )
	{
		delay( 500 );
		Serial.print( "." );
	}

	Serial.println();
	Serial.print( "Connected to " );
	Serial.println( ssid );

	Serial.print( "IP address: " );
	Serial.println( WiFi.localIP() );

	udp.begin( udpPort );
	Serial.printf( "Now listening at IP %s, UDP port %d\n",
				   WiFi.localIP().toString().c_str(), udpPort );

	// Uncomment to refresh files state:
	// clearFile( LittleFS, "/comm_logs.txt" );
	// clearFile( LittleFS, "/info.txt" );

	String && info = readFile( LittleFS, "/info.txt" );

	if ( info == "" )
	{
		// Same as:
		// all_info.emplace( "esp1", Data {} );
		// all_info.emplace( "esp2", Data {} );
		// all_info.emplace( "esp3", Data {} );
		// ....

		for ( const auto & name : all_names )
		{
			all_info.emplace( name, Data {} );
		}
	}
	else
	{
		JsonDocument doc;

		deserializeJson( doc, info );

		JsonArray array = doc.as< JsonArray >();
		for ( JsonVariant v : array )
		{
			std::string id		 = v[ "id" ];
			std::string state	 = v[ "state" ];
			int			time_cnt = v[ "time_cnt" ];

			all_info[ id ] = Data { state, time_cnt };
		}
	}

	server.on( "/info", HTTP_GET, handle_info );
	server.on( "/comms", HTTP_GET, handle_comms );

  auto cfg = esp_pthread_get_default_config();

  cfg.stack_size	= 4 * 1024;
  cfg.inherit_cfg = true;
  cfg.prio		= 1;

  esp_pthread_set_cfg( &cfg );

	std::thread { udp_server }.detach();
	std::thread { inference_task }.detach();
	std::thread { print_info }.detach();

	esp_pthread_get_cfg( &cfg );
	cfg.prio = 5;
	esp_pthread_set_cfg( &cfg );

	std::thread {
		[]()
		{
			while ( true ) { server.handleClient(); }
		}
	}.detach();

	server.begin();
}

void handle_info()
{
	Serial.printf( "handle_info on core: %li, priority: %lu\n\n",
				   xPortGetCoreID(), uxTaskPriorityGet( NULL ) );
	server.send( 200, "application/json", readFile( LittleFS, "/info.txt" ) );
}

void handle_comms()
{
	Serial.printf( "handle_comms on core: %li, priority: %lu\n\n",
				   xPortGetCoreID(), uxTaskPriorityGet( NULL ) );
	server.send( 200, "text/plain", readFile( LittleFS, "/comm_logs.txt" ) );
}

void udp_server()
{
	while ( true )
	{
		Serial.printf( "UDP_Server on core: %li, priority: %lu\n\n",
					   xPortGetCoreID(), uxTaskPriorityGet( NULL ) );
		int sz = udp.parsePacket();

		if ( not sz )
		{
			std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
			continue;
		}

		uint8_t buffer[ 1024 ] = "";

		if ( udp.read( buffer, sizeof( buffer ) ) > 0 )
		{
			Serial.printf( "Received msg: %s\n\n", buffer );
			{
				String log = "[ ";
				log += udp.remoteIP().toString();
				log += ":";
				log += String( udp.remotePort() );
				log += " ]: ";
				log += ( const char * )buffer;

				appendFile( LittleFS, "/comm_logs.txt", log.c_str() );
			}

			JsonDocument doc;
			deserializeJson( doc, buffer );

			std::string msg_id = doc[ "id" ] | "";

			if ( msg_id == "admin" )
			{
				String msg_cmd = doc[ "cmd" ] | "";

				if ( msg_cmd == "reset" )
				{
					std::lock_guard< std::mutex > guard( info_mutex );

					// erase file
					clearFile( LittleFS, "/comm_logs.txt" );
					clearFile( LittleFS, "/info.txt" );

					for ( auto & [ id, data ] : all_info ) { data = Data {}; }
					continue;
				}
			}
			else
			{
				std::string msg_state = doc[ "state" ] | "";
				int			time_cnt  = doc[ "time" ] | -1;

				if ( msg_id == "" or msg_state == "" or time_cnt < 0 )
				{
					continue;
				}

				{
					std::lock_guard< std::mutex > guard( info_mutex );

					if ( auto it = all_info.find( msg_id );
						 it != all_info.end() and
						 it->second.time_cnt < time_cnt )
						all_info[ msg_id ] = Data { msg_state, time_cnt };
				}
			}
		}
	}
}

void inference_task()
{
	const char *   state[] = { "fire", "not fire" };
	constexpr auto NUM	   = 10;

	std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

	while ( true )
	{
		Serial.printf( "Inference Task on core: %li, priority: %lu\n\n",
					   xPortGetCoreID(), uxTaskPriorityGet( NULL ) );

		{
			// Generate random input
			std::vector< std::vector< float > > input {};
			input.reserve( NUM );

			for ( auto i = 0; i < NUM; ++i )
			{
				float num = dis( gen );

				input.push_back( { num } );
			}

			// Perform inference
			auto output = model.forward( input );

			Serial.print( "Output size: " );
			Serial.println( output.size() );

			// Print the output
			Serial.println( "Results:\n" );
			for ( size_t i = 0; i < output.size(); ++i )
			{
				Serial.print( "Input: " );
				print_row( input[ i ] );
				Serial.print( "\tOutput: " );
				print_row( output[ i ] );
				Serial.print( "\tReal: " );
				Serial.printf( "% 2.5lf", std::sin( input[ i ][ 0 ] * PI ) );
				Serial.println();
			}
			Serial.println( "\n" );
		}

		{
			JsonDocument doc;
			{
				std::lock_guard< std::mutex > guard( info_mutex );

				doc[ "id" ]	   = my_name;
				doc[ "state" ] = state[ rand() % 2 ];
				doc[ "time" ]  = all_info[ my_name ].time_cnt + 1;

				all_info[ my_name ] = Data { doc[ "state" ], doc[ "time" ] };
			}

			udp.beginPacket( udpAddress, udpPort );
			serializeJson( doc, udp );
			udp.endPacket();

			// udp.beginPacket( udpAddress, udpPort + 1 );
			// serializeJson( doc, udp );
			// udp.endPacket();
		}

		std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
	}
}

void print_info()
{
	while ( true )
	{
		{
			std::lock_guard< std::mutex > guard( info_mutex );

			Serial.printf( "Printing info on core: %li, priority: %lu\n\n",
						   xPortGetCoreID(), uxTaskPriorityGet( NULL ) );

			for ( const auto & [ id, data ] : all_info )
			{
				Serial.printf( "Id: %s, state: %s, time: %i\n", id.c_str(),
							   data.state.c_str(), data.time_cnt );
			}
			Serial.println();

			std::unordered_map< std::string, int > cnt;
			for ( const auto & [ id, data ] : all_info )
			{
				cnt[ data.state ] += 1;
			}

			auto & pred = std::max_element( cnt.cbegin(), cnt.cend(),
											[]( const auto & a, const auto & b )
											{ return a.second < b.second; } )
							  ->first;

			Serial.printf( "Prediction: %s\n\n", pred.c_str() );

			JsonDocument doc;
			JsonArray	 array = doc.to< JsonArray >();

			for ( const auto & [ id, data ] : all_info )
			{
				auto obj = doc.add< JsonObject >();

				obj[ "id" ]		  = id;
				obj[ "state" ]	  = data.state;
				obj[ "time_cnt" ] = data.time_cnt;
			}

			String info {};
			serializeJson( doc, info );

			writeFile( LittleFS, "/info.txt", info.c_str() );
		}

		Serial.print( "Total Heap: " );
		Serial.println( ESP.getHeapSize() );
		Serial.print( "Min Free Heap: " );
		Serial.println( ESP.getMinFreeHeap() );
		Serial.print( "Max Alloc Heap: " );
		Serial.println( ESP.getMaxAllocHeap() );
		Serial.println();

		// Wait for 2 second
		std::this_thread::sleep_for( std::chrono::seconds( 2 ) );
	}
}

void loop() {}
