#include "model_params.hpp"	   // Include the header file containing weights and biases
#include <vector>

class MyModel
{
	public:

	MyModel() : initialized { false } {}

	void init()
	{
		if ( not initialized )
		{
			initializeModel();
			initialized = true;
		}
	}

	auto total_used_mem() -> size_t
	{
		size_t total = 0;
		total +=
			weights.size() * weights[ 0 ].size() * weights[ 0 ][ 0 ].size();
		total += biases.size() * biases[ 0 ].size();
		total *= sizeof( float );
		return total;
	}

	auto forward( const std::vector< float > & _input ) -> std::vector< float >
	{
		std::vector< std::vector< float > > input = { _input };
		return forward( input[ 0 ] );
	}

	auto forward( std::vector< std::vector< float > > input )
		-> std::vector< std::vector< float > >
	{
		// Perform forward pass
		for ( size_t i = 0; i < weights.size(); ++i )
		{
			input = linear( input, weights[ i ], biases[ i ] );
			if ( i < weights.size() - 1 ) { relu( input ); }
		}
		return input;
	}

	private:

	std::vector< std::vector< std::vector< float > > > weights;
	std::vector< std::vector< float > >				   biases;
	bool											   initialized = false;

	void initializeModel()
	{
		// Load weights and biases
		// Replace the hardcoded values with the values from model_params.hpp
		// Example: weights.push_back(weights_0);
		//          biases.push_back(biases_0);

		ModelParams mp;

		weights.push_back( mp.weights_0 );
		weights.push_back( mp.weights_1 );
		weights.shrink_to_fit();

		biases.push_back( mp.biases_0 );
		biases.push_back( mp.biases_1 );
		biases.shrink_to_fit();
	}

	/**
	 * Linear layer:
	 *
	 * output = input * weight^T + bias
	 *
	 * input: [batch_size, input_size]
	 * weight: [output_size, input_size]
	 * bias: [output_size]
	 * output: [batch_size, output_size]
	 */
	auto linear( const std::vector< std::vector< float > > & input,
				 const std::vector< std::vector< float > > & weight,
				 const std::vector< float > &				 bias )
		-> std::vector< std::vector< float > >
	{
		std::vector< std::vector< float > > output;
		output.reserve( input.size() );

		for ( size_t i = 0; i < input.size(); ++i )
		{
			std::vector< float > row;
			for ( size_t j = 0; j < weight.size(); ++j )
			{
				float val = 0.0;
				for ( size_t k = 0; k < input[ i ].size(); ++k )
				{
					val += input[ i ][ k ] * weight[ j ][ k ];
				}
				row.push_back( val + bias[ j ] );
			}
			output.push_back( row );
		}

		return output;
	}

	/**
	 * ReLU activation function:
	 *
	 * output = max(0, input)
	 *
	 * input: [batch_size, input_size]
	 */
	void relu( std::vector< std::vector< float > > & input )
	{
		for ( auto & row : input )
		{
			for ( auto & val : row ) { val = std::max( 0.0f, val ); }
		}
	}
};
