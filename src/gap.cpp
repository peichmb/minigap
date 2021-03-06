#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <cstdio>
#include <vector>
#include "gap.h"

namespace {

	// PI
	const double PI = 3.14159265358979323846;

	// Minimum tree diameter
	const double DIAMETER_MIN = 0.5;

	// Seed to initialize the random number generator
	const int SEED = 74837891;

	// Cherry trees establishment cutoff
	const double CHERRY_CUTOFF = 55.;

	// Birch trees establishment cutoff
	const double BIRCH_CUTOFF = 1000.;

	// Light extinction coefficient
	const double K_EXT = 1/6000.;

	// This is a property of the SITE, when done creating stand move up the hierarchy
	// I'm not even using it in the current version, delete probably.
	const double degd = 2500.;

	// Vector containing the static PFT parameters
	std::vector<Pft> pft_vector;

	// Vectors of ids of shade tolerant/intolerant PFTs
	std::vector<int> shade_tolerant_pfts;
	std::vector<int> cherry_pfts;
	std::vector<int> birch_pfts;

	// Simulation year (maybe in framework)
	int simulation_year;

	std::string output_header;

}

// PFT counter
int Pft::npft = 0;

// --------------------//
// 	struct PFT     //
// --------------------//

Pft::Pft(const char* name, double g, double c, double age_max, tree_type type,
    double d_max, double h_max, double b2, double b3,
    double degd_min, double degd_max, double wmin, double wmax) :
    name(name), id(Pft::npft), g(g), c(c), age_max(age_max), type(type), d_max(d_max), h_max(h_max),
    b2(b2), b3(b3), degd_min(degd_min), degd_max(degd_max), wmin(wmin), wmax(wmax)
{
	Pft::npft++;
}

// --------------------//
// 	class Tree     //
// --------------------//

Tree::Tree(int pft_id) : pft(pft_vector[pft_id]), pft_id(pft_id) {
	// WORKSHOP: Initialize these to zero to let them catch the bug.
	tree_age = 0;
	double small_random_addition = ((double)rand()/RAND_MAX) * 0.1 * DIAMETER_MIN;
	d = DIAMETER_MIN + small_random_addition;
	d_change = 1.; // Ensures that the tree doesn't die right after it's born
	sla = 0.;
	update_height();
	update_weight(); // Probably don't need to do this here
	update_basal_area(); // Probably don't need to do this here
}

void Tree::growth() {

	// Calculate potential change in tree diameter over 1 year (B79, Eq. 5)
	d_change = pft.g*d*(1.-d*h/(pft.d_max * pft.h_max))
			/ (274 + 3.*pft.b2*d - 4.*pft.b3*d*d);

	// Environmental factors affecting growth
	double f_env = 1;
	d_change = d_change*f_env*r_light();

	// Update diameter
	d = d + d_change;
	// Update height
	update_height();
	// Update weight
	update_weight();
	// Update basal_area
	update_basal_area();
	// Update age
	tree_age++;
}

double Tree::diameter() {
	return d;
}

double Tree::height() {
	return h;
}

double Tree::weight() {
	return w;
}

double Tree::basal_area() {
	return ba;
}

int Tree::age() {
	return tree_age;
}

double Tree::diameter_change() {
	return d_change;
}

void Tree::set_sla(double s) {
	sla = s;
}

double Tree::get_sla() {
	return sla;
}

void Tree::update_height() {
	h = 137 + pft.b2*d - pft.b3*d*d;
}

void Tree::update_weight() {
	w = pft.c*d*d;
}

void Tree::update_basal_area() {
	ba = 0.25*PI*d*d;
}

double Tree::r_light() {

	double al = exp(-K_EXT*sla);

	if (pft.type == SHADE_TOLERANT) {
		return std::max(1. - exp(-4.64*(al-0.05)),0.);
	} else {
		return std::max(2.24*(1. - exp(-1.136*(al-0.08))),0.);
	}
}

Tree::~Tree() {}


// --------------------- //
// 	class Plot       //
// --------------------- //

Plot::Plot() {
	weight = 0.;
	basal_area = 0.;
}

Plot::~Plot() {}

void Plot::advance() {

	// Add new trees
	birth();
	// Kill trees
	kill();
	// Grow remaining trees
	growth();
}

std::string Plot::info() {

	char buffer[1000];
	sprintf(buffer, "%6d %6d %12.3f %12.3f", simulation_year, trees.size(), weight, basal_area);
	for (int i=0; i<npft(); i++) {
		int ntrees_pft = 0;
		double weight_pft = 0.;
		double basal_area_pft = 0.;
		for (auto& tree : trees) {
			if (tree.pft.id == i) {
				ntrees_pft++;
				weight_pft += tree.weight();
				basal_area_pft += tree.basal_area();
			}
		}
		sprintf(buffer, "%s %6d %12.3f %12.3f", buffer,
				ntrees_pft, weight_pft, basal_area_pft);
	}

	return std::string(buffer);
}

void Plot::birth() {

	// TODO: PFT selection must be based on bioclimatic limits. See B72, p. 857.

	// First the shade tolerant plants
	int new_shade_tolerant_pft_index = rand() % shade_tolerant_pfts.size();
	int new_shade_tolerant_pft = shade_tolerant_pfts[new_shade_tolerant_pft_index];
	int number_of_shade_tolerant_saplings = rand() % 3;

	// Append new shade tolerant saplings to end of the list
	for (int i=0; i<number_of_shade_tolerant_saplings; i++) {
		trees.push_back(Tree(new_shade_tolerant_pft));
	}

	// Now the shade intolerant ones
	if (weight < CHERRY_CUTOFF) {
		// Introduce 60 to 75 cherry trees
		int number_of_new_cherries = 60 + rand()%16;
		for (int i=0; i<number_of_new_cherries; i++) {
			int new_cherry_pft = cherry_pfts[rand()%cherry_pfts.size()];
			trees.push_back(Tree(new_cherry_pft));
		}
	} else if (weight < BIRCH_CUTOFF) {
		// Introduce between 0 and 13 birch trees
		int number_of_new_birches = rand()%14;
		for (int i=0; i<number_of_new_birches; i++) {
			int new_birch_pft = birch_pfts[rand()%birch_pfts.size()];
			trees.push_back(Tree(new_birch_pft));
		}
	}
}

void Plot::kill() {

	// First death mechanism
	for (std::list<Tree>::iterator it=trees.begin(); it != trees.end(); it++) {
		Tree& tree = *it;
		// Probability of dying this year
		double p = 1. - pow((1. - 4./(double)tree.pft.age_max), tree.age());
		if ((double)rand()/RAND_MAX < p) {
			// Remove tree from list
			it = --trees.erase(it);
		}
	}

	// Second death mechanism
	for (std::list<Tree>::iterator it=trees.begin(); it != trees.end(); it++) {
		Tree& tree = *it;
		// Probability of dying this year
		// Bug for debugging training: forget the (double) cast
		if (tree.diameter_change() < 0.01 && (double)rand()/RAND_MAX < 0.368) {
			// Remove tree from list and set iterator to previous element
			// so that it points to the correct element when the
			// iterator is incremented
			it = --trees.erase(it);
		}
	}
}

void Plot::growth() {

	// First calculate shading leaf area for each tree
	for (auto& tree : trees) {
		double sla = 0;
		for (auto& tree1 : trees) {
			if (tree1.height() > tree.height()) {
				sla += tree1.weight();
			}
		}
		tree.set_sla(sla);
	}

	weight = 0.;
	basal_area = 0.;
	for (auto& tree : trees) {
		// Grow tree
		tree.growth();
		// Add tree's weight and basal area to plot's total
		weight += tree.weight();
		basal_area += tree.basal_area();
	}
}


// --------------------- //
// 	class Forest     //
// --------------------- //

Forest::Forest(int nplots) {
	for (int i=0; i<nplots; i++) {
		plots.push_back(Plot());
	}
}

int Forest::nplots() {
	return plots.size();
}

void Forest::advance() {
	for (auto& plot : plots) {
		plot.advance();
	}
}

void Forest::dump_output() {
	for (auto& plot : plots) {
		std::cout << plot.info() << "\n";
	}
}

Forest::~Forest() {
	//
}


// ---------------------------- //
// 	module functions	//
// ---------------------------- //

void initialize_gap() {

	// Populate pft vector
	// Parameters from Botkin et al. (1972)
	//
	// 				Name        g      c  age_max              type  d_max  h_max    b2     b3 dgde_min dgde_max  wmin  wmax
	pft_vector.push_back(Pft ("Sugar maple",   170., 1.57 ,     200,   SHADE_TOLERANT, 152.5, 4011., 50.9, 0.167,   2000.,   6300., 300.,  -1.));
	pft_vector.push_back(Pft ("Beech", 	   150., 2.20 ,     300,   SHADE_TOLERANT, 122.0, 3660., 57.8, 0.237,   2100.,   6000., 300.,  -1.));
	pft_vector.push_back(Pft ("Yellow birch",  100., 0.486,     300,            BIRCH, 122.0, 3050., 47.8, 0.196,   2000.,   5300., 250.,  -1.));
	pft_vector.push_back(Pft ("White ash",     130., 1.75 ,     100,   SHADE_TOLERANT,  50.0, 2160., 80.2, 0.802,   2100.,  10700., 320.,  -1.));
	pft_vector.push_back(Pft ("Mt. maple",     100., 1.13 ,      25,   SHADE_TOLERANT,  13.5,  500., 53.8, 2.0  ,   2000.,   6300., 320.,  -1.));
	pft_vector.push_back(Pft ("Striped maple", 150., 1.75 ,      30,   SHADE_TOLERANT,  22.5, 1000., 76.6, 1.70 ,   2000.,   6300., 320.,  -1.));
	pft_vector.push_back(Pft ("Pin cherry",    200., 2.45 ,      30,           CHERRY,  28.5, 1126., 70.6, 1.26 ,   1100.,   8000., 190.,  -1.));
	pft_vector.push_back(Pft ("Choke cherry",  150., 2.45 ,      20,           CHERRY,  10.0,  500., 72.6, 3.63 ,    600.,  10000., 155.,  -1.));
	pft_vector.push_back(Pft ("Balsam Fir",    200., 2.5  ,      80,   SHADE_TOLERANT,  50.0, 1830., 67.9, 0.679,   1100.,   3700., 190.,  -1.));
	pft_vector.push_back(Pft ("Spruce",         50., 2.5  ,     350,   SHADE_TOLERANT,  50.0, 1830., 67.9, 0.679,    600.,   3700., 190.,  -1.));
	pft_vector.push_back(Pft ("White birch",   140., 0.486,      80,            BIRCH,  46.0, 1830., 73.6, 0.800,   1100.,   3700., 190., 600.));
	pft_vector.push_back(Pft ("Mt. ash",       150., 1.75 ,      30,   SHADE_TOLERANT,  10.0,  500., 72.6, 3.63 ,   2000.,   4000., 300.,  -1.));
	pft_vector.push_back(Pft ("Red maple",     240., 1.75 ,     150,   SHADE_TOLERANT, 152.5, 3660., 46.3, 0.152,   2000.,  12400., 300.,  -1.));

	// Vectors containing the indices of shade (in)tolerant trees on the pft_vector
	for (int i=0; i<pft_vector.size(); i++) {
		Pft& pft = pft_vector[i];
		if (pft.type == SHADE_TOLERANT) {
			shade_tolerant_pfts.push_back(i);
		} else if (pft.type == CHERRY) {
			cherry_pfts.push_back(i);
		} else if (pft.type == BIRCH) {
			birch_pfts.push_back(i);
		}
	}

	//for (auto& pft : pft_vector) {
	//	std::cout << pft.id << " " << pft.name << "\n";
	//}

	// Initialize random number generator
	srand(SEED);

	// Set simulation year to 0
	simulation_year = 0;

	// Output header
	// -------------
	char buffer[1000];
	sprintf(buffer,"%6s|%-32s", "", " PLOT");
	for (auto& pft : pft_vector) {
		sprintf(buffer, "%s| %-31s", buffer, pft.name.c_str());
	}
	sprintf(buffer, "%s|\n%-6s|%-6s|%-12s|%-12s", buffer, " Year", " #tr", " weight", " b. area");
	for (auto& pft : pft_vector) {
		sprintf(buffer,  "%s|%-6s|%-12s|%-12s",
			buffer,
			" #tr",
			" weight",
			" b. area");
	}
	output_header = buffer;
	output_header += "|";
}

int npft() {
	return Pft::npft;
}

void increase_simulation_year() {
	simulation_year++;
}

std::string get_header() {
	return output_header;
}

