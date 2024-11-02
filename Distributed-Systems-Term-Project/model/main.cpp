#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

#include "model.hpp"

const float PI = std::acos( -1 );

template < typename T >
void print_row( const std::vector< T > & row )
{
	// also print comma in a pretty way
	std::cout << "[ ";
	for ( size_t i = 0; i < row.size(); ++i )
	{
		std::cout << row[ i ];
		if ( i < row.size() - 1 ) { std::cout << ", "; }
	}
	std::cout << " ]";
}

int main()
{
	// Instantiate the model
	MyModel model;
	model.init();

	// Print total memory usage
	std::cerr << "Total memory usage: " << model.total_used_mem() << " bytes"
			  << std::endl;

	constexpr auto NUM = 1'000;

	// Generate random input
	std::vector< std::vector< float > >		input;
	std::random_device						rd;
	std::mt19937							gen( rd() );
	std::uniform_real_distribution< float > dis( -1.0f, 1.0f );

	for ( auto i = 0; i < NUM; ++i )
	{
		float num = dis( gen );

		input.push_back( { num } );
	}

	// Measure the time
	auto start = std::chrono::high_resolution_clock::now();

	// Perform inference
	auto output = model.forward( input );

	std::cout << "Output size: " << output.size() << std::endl;

	// Print the output
	std::cout << "Results:\n";
	for ( size_t i = 0; i < output.size(); ++i )
	{
		std::cout << "Input: ";
		print_row( input[ i ] );
		std::cout << "\tOutput: ";
		print_row( output[ i ] );
		std::cout << "\tReal: " << std::sin( input[ i ][ 0 ] * PI );
		std::cout << std::endl;
	}

	auto end = std::chrono::high_resolution_clock::now();

	std::cerr << "Time: "
			  << std::chrono::duration_cast< std::chrono::milliseconds >(
					 end - start )
					 .count()
			  << "ms" << std::endl;
}
