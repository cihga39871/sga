//-----------------------------------------------
// Copyright 2009 Wellcome Trust Sanger Institute
// Written by Jared Simpson (js18@sanger.ac.uk)
// Released under the GPL license
//-----------------------------------------------
//
// BWT.cpp - Burrows Wheeler transform of a generalized suffix array
//
#include "BWT.h"
#include "Timer.h"
#include <istream>

// macros
#define OCC(c,i) m_occurance.get(m_bwStr, (c), (i))
#define PRED(c) m_predCount.get((c))

// Parse a BWT from a file
BWT::BWT(const std::string& filename)
{
	std::ifstream in(filename.c_str());
	checkFileHandle(in, filename);	
	in >> *this;
	in.close();
}

// Construct the BWT from a suffix array
BWT::BWT(const SuffixArray* pSA, const ReadTable* pRT)
{
	Timer timer("BWT Construction");
	size_t n = pSA->getSize();
	m_numStrings = pSA->getNumStrings();
	m_bwStr.resize(n);

	// Set up the bwt string and suffix array from the cycled strings
	for(size_t i = 0; i < n; ++i)
	{
		SAElem saElem = pSA->get(i);
		const SeqItem& si = pRT->getRead(saElem.getID());

		// Get the position of the start of the suffix
		uint64_t f_pos = saElem.getPos();
		uint64_t l_pos = (f_pos == 0) ? si.seq.length() : f_pos - 1;
		m_bwStr[i] = (l_pos == si.seq.length()) ? '$' : si.seq.get(l_pos);
	}

	// initialize the occurance table
	m_occurance.initialize(m_bwStr, DEFAULT_SAMPLE_RATE);

	// Calculate the C(a) array
	
	// Calculate the total number of occurances of each character in the BW str
	AlphaCount tmp;
	for(size_t i = 0; i < m_bwStr.size(); ++i)
	{
		tmp.increment(m_bwStr[i]);
	}

	m_predCount.set('$', 0);
	m_predCount.set('A', tmp.get('$')); 
	m_predCount.set('C', m_predCount.get('A') + tmp.get('A'));
	m_predCount.set('G', m_predCount.get('C') + tmp.get('C'));
	m_predCount.set('T', m_predCount.get('G') + tmp.get('G'));
}

// Compute the last to first mapping for this BWT
size_t BWT::LF(size_t idx) const
{
	return m_bwStr[idx] != '$' ? PRED(m_bwStr[idx]) + OCC(m_bwStr[idx], idx) : 0;
}

// Perform a exact search for the string w using the backwards algorithm
void BWT::backwardSearch(std::string w) const
{
	std::cout << "Searching for " << w << "\n";
	int len = w.size();
	int j = len - 1;
	char curr = w[j];
	int r_lower = PRED(curr);
	int r_upper = r_lower + OCC(curr, m_bwStr.size() - 1) - 1;
	--j;
	std::cout << "Starting point: " << r_lower << "," << r_upper << "\n";
	for(;j >= 0; --j)
	{
		curr = w[j];
		printf("RL = C(%c) + O(%c,%d) + %zu\n", curr, curr, r_lower - 1, m_numStrings); 
		printf("RU = C(%c) + O(%c,%d)\n", curr, curr, r_upper); 
		printf("RL = %zu + %zu + %zu\n", PRED(curr), OCC(curr, r_lower - 1), m_numStrings); 
		printf("RU = %zu + %zu\n", PRED(curr), OCC(curr, r_upper)); 
		r_lower = PRED(curr) + OCC(curr, r_lower - 1);
		r_upper = PRED(curr) + OCC(curr, r_upper) - 1;
		printf("Curr: %c, Interval now: %d,%d\n", curr, r_lower, r_upper);
	}

	std::cout << "Interval found: " << r_lower << "," << r_upper << "\n";
}

// Perform a search for hits to read prefixes using a backward search algorithm
void BWT::getPrefixHits(size_t readIdx, std::string w, int minOverlap, bool targetRev, bool queryRev, HitVector* pHits) const
{
	// Initialize the search
	int len = w.size();
	int j = len - 1;
	char curr = w[j];
	size_t r_lower = PRED(curr);
	size_t r_upper = r_lower + OCC(curr, m_bwStr.size() - 1) - 1;
	--j;
	//std::cout << "Searching for string: " << w << "\n";
	//printf("Starting point: %zu,%zu\n", r_lower, r_upper);
	//std::cout << "Starting point: " << r_lower << "," << r_upper << "\n";
	for(;j >= 0; --j)
	{
		curr = w[j];

		//printf("RL = C(%c) + O(%c,%zu) + %zu\n", curr, curr, r_lower - 1, m_numStrings); 
		//printf("RU = C(%c) + O(%c,%zu)\n", curr, curr, r_upper); 
		//printf("RL = %zu + %zu + %zu\n", PRED(curr), OCC(curr, r_lower - 1), m_numStrings); 
		//printf("RU = %zu + %zu\n", PRED(curr), OCC(curr, r_upper));

		r_lower = PRED(curr) + OCC(curr, r_lower - 1);
		r_upper = PRED(curr) + OCC(curr, r_upper) - 1;

		//printf("Curr: %c, Interval now: %zu,%zu\n", curr, r_lower, r_upper);
		int overlapLen = len - j;
		if(overlapLen >= minOverlap)
		{
			// Output the hits where the suffix of w has matched a proper prefix 
			// (starting from the begining of the string) of some other string
			// These suffixes can be calculated using the fm-index like any other interval
			size_t t_lower = PRED('$') + OCC('$', r_lower - 1);
			size_t t_upper = PRED('$') + OCC('$', r_upper) - 1;
			for(size_t sa_idx = t_lower; sa_idx <= t_upper; ++sa_idx)
				pHits->push_back(Hit(readIdx, sa_idx, j, overlapLen, targetRev, queryRev));
		}
	}
}

void BWT::validate() const
{
	std::cerr << "Warning BWT validation is turned on\n";
	m_occurance.validate(m_bwStr);
}

// Output operator
std::ostream& operator<<(std::ostream& out, const BWT& bwt)
{

	out << bwt.m_numStrings << "\n";
	out << bwt.m_bwStr.size() << "\n";
	out << bwt.m_bwStr << "\n";
	out << bwt.m_predCount << "\n";
	out << bwt.m_occurance;
	return out;
}


// Input operator
std::istream& operator>>(std::istream& in, BWT& bwt)
{
	in >> bwt.m_numStrings;
	size_t n;
	in >> n;
	bwt.m_bwStr.resize(n);
	in >> bwt.m_bwStr;
	in >> bwt.m_predCount;
	in >> bwt.m_occurance;
	return in;
}

// write the suffix array to a file
void BWT::write(std::string& filename)
{
	std::ofstream out(filename.c_str());
	out << *this;
	out.close();
}


// Print the BWT
void BWT::print(const ReadTable* pRT, const SuffixArray* pSA) const
{
	std::cout << "i\tL(i)\tO(-,i)\tSUFF\n";
	for(size_t i = 0; i < m_bwStr.size(); ++i)
	{
		std::cout << i << "\t" << m_bwStr[i] << "\t" << m_occurance.get(m_bwStr, i) << pSA->getSuffix(i, pRT) << "\n";
	}
}

// Print information about the BWT
void BWT::printInfo() const
{
	size_t o_size = m_occurance.getByteSize();
	size_t p_size = sizeof(m_predCount);

	size_t bwStr_size = sizeof(m_bwStr) + m_bwStr.size();
	size_t offset_size = sizeof(m_numStrings);
	size_t total_size = o_size + p_size + bwStr_size + offset_size;
	double total_mb = ((double)total_size / (double)(1024 * 1024));
	printf("BWT Size -- OCC: %zu C: %zu Str: %zu Misc: %zu TOTAL: %zu (%lf MB)\n",
			o_size, p_size, bwStr_size, offset_size, total_size, total_mb);
	printf("N: %zu Bytes per suffix: %lf\n", m_bwStr.size(), (double)total_size / m_bwStr.size());
}
