// VoxelGridCreator.cpp : Defines the entry point for the console application.
//radii and rings per atom parallel arrays
//ideally we should be able to determine if any point is in a certain voxel, and where that is relative to the rest of the grid
//keep thickness in mind.. for example if size >= 1 then add .499999999999 or .1 <= size < 1 then add 0.04999999999 and so on
//we will read all points from the file and then determine our x,y,z transforms so that we do not need to transform when resizing
//this will also help us with resizing, becuase if the transformed point does not lie in any of our existing voxels we double size until it does
//getting us to our needed size either at the same rate at worst case or with one point in best case.
//we will open some mole file, populate a vector of points and a corresponding vector of atoms and once we know the most neg and pos x,y,z
//we can make our grid such that it will contain the transformed x,y,z. So whichever is the most pos side, x,y,z; If the cube can fit the one that
//goes farthest out, then it will contain all 3. And so that there is a buffer between the nucleus and the grid I will put in a buffer
//such that we can more easily get in our point cloud representations.
/*	To do:
		- test creating grid and populating voxels
		- test reading from file and populating grid
		- test entering a point and finding the corresponding voxel
		- speak with samudio for understanding of center point
*/

#include <iostream>
#include <algorithm>
#include <vector>

struct Point
{
	double x, y, z;

	Point() { x = y = z = 0.0; }		//default to origin
	Point(double a, double b, double c) { x = a; y = b; z = c; }
	Point(const Point& p) { x = p.x; y = p.y; z = p.z; }
};

struct Atom
{
	double *radius; //each ring will have a radius
	uint32_t rings;  //each ring represents a hollowed out sphere
	Point nucleus; //location of nucleus

	Atom() { radius = 0; rings = 0; nucleus.x = nucleus.y = nucleus.z = 0.0; } //default radius pointer to null, number of rings to 0,
																				//and nucleus location to (0,0,0)
	~Atom() { if (radius) delete[] radius; }
};

struct Voxel
{
	uint64_t bPixels, gPixels, rPixels;
	double sideLength;					//one side length because cube length = width = height
	Point upBackLeftCorner;				//use to get some kind of grasp on orientation

	//default to 0 pixels, upper back left corner of voxel cube to be (0,0,0) and sideLength of 0
	Voxel() 
	{ 
		bPixels = gPixels = rPixels = 0; 
		upBackLeftCorner = Point(0.0 ,0.0 ,0.0); 
		sideLength = 0.0;
	}

	Voxel(double length)
	{
		bPixels = gPixels = rPixels = 0;
		upBackLeftCorner = Point(0.0, 0.0, 0.0);
		sideLength = length;
	}

	Voxel(double length, const Point& upBackLeft)
	{
		bPixels = gPixels = rPixels = 0;
		upBackLeftCorner = upBackLeft;
		sideLength = length;
	}

	Voxel(double length, const Point& upBackLeft, uint64_t b, uint64_t g, uint64_t r)
	{
		bPixels = b;
		gPixels = g;
		rPixels = r;
		upBackLeftCorner = upBackLeft;
		sideLength = length;
	}
};

//grid square of 3 dimensional voxel cubes
struct Grid
{
	Voxel*** arr;	//3 dimensional dinamically allocated array of voxels for the grid
	uint32_t numOfVoxels; //grid dimensions determined by numOfVoxels, i.e numOfVoxels is num of rows and columns in grid since cube
						//and voxel size will be its own seperate thing so user will say, oh I have voxels of .5 Angstrum so maybe
						//I need x amount of voxels or we will figure out calculation of grid dimensions based on # of input points
	Point center;

	Grid() { arr = 0; numOfVoxels = 0; center = Point(0.0, 0.0, 0.0); }
	
	//only voxels are provided, so center is assumed to be origin
	Grid(uint32_t voxels, double voxelSideLength)
	{
		numOfVoxels = voxels;
		center = Point(0.0, 0.0, 0.0);

		//assign arr to point to an array of voxel pointer to pointer, each voxel pointer to a pointer to a voxel pointer array, and each voxel pointer to an array of voxels
		if (voxels > 0)
		{
			//combine with for-loop below later for cleanliness and speed
			arr = new Voxel**[numOfVoxels]; ///ask tak if this gurantees 64 bits unsigned and why, use 32 bit for indexing
			for (uint32_t i = 0; i < numOfVoxels; ++i)
			{
				arr[i] = new Voxel*[(unsigned)numOfVoxels];
				for (uint32_t j = 0; j < numOfVoxels; ++j)
					arr[i][j] = new Voxel[(unsigned)numOfVoxels];
			}

			//set all voxel sideLengths to length
			for (uint32_t i = 0; i < numOfVoxels; ++i)
			{
				for (uint32_t j = 0; j < numOfVoxels; ++j)
				{
					for(uint32_t k = 0; k < numOfVoxels; ++k)
						arr[i][j][k].sideLength = voxelSideLength;

				}
			}
		}
	}
	
	//instantuate with voxels and then set center to P, might in the near future need some validation for the center point
	Grid(uint32_t voxels, double voxelSideLength, const Point& p)
		:Grid(voxels, voxelSideLength) {
		center = p;
	}
	
	//will need to go through and delete all allocated memory
	~Grid() 
	{
		if (arr)
		{
			for (uint32_t i = 0; i < numOfVoxels; ++i)
			{
				for(uint32_t j = 0; j < numOfVoxels; ++j)
					delete[] arr[i][j];
				delete[] arr[i];
			}
		}
	} 

	//to set num of voxels, we also want to know the voxel side length... in progress
	/*void setVoxels(uint32_t voxels)
	{	
		
		//might want to either be allowed to only set once or transfer everthing to a new grid
		//if no grid then create new grid with either origin or previously specified center
		if (arr)    
		{	
			//Later add promt to let user know data may be lost if we resize the cube smaller
			//temp arr of voxels** of new size
			Voxel*** temp = new Voxel**[voxels]; 
			//copy existing voxels
			for (uint32_t i = 0; i < numOfVoxels; ++i)
			{
				temp[i] = new Voxel*[voxels];
				for (uint32_t j = 0; j < numOfVoxels; ++j)  //use numOfVoxels because voxels will cause out of range exception on arr
				{
					temp[i][j] = new Voxel[voxels];
					for (uint32_t k = 0; k < numOfVoxels; ++k)
					{
						temp[i][j][k] = arr[i][j][k];
					}
				}
			}
			
			//all existing voxels have been put in their corresponding locations
		}
		else {}
			//*this = Grid(voxels, length, center);
	}*/
};

//this function will return a pointer to whatever voxel our point lies in whatever .... in progress
Voxel* FindVoxel(Grid grid)
{
	uint32_t f_pos, s_pos, t_pos;
	if (grid.arr)
	{
		f_pos = s_pos = t_pos = 0;

		//...
		//...
		//...

		return &grid.arr[f_pos][s_pos][t_pos];
	}
	else
		return 0;
}

int main()
{
	const int size = 10;
	Grid *g = new Grid(size, 0.5); //dynamic because it might be large
	


	/*//check if all of grid is there and if a component is initialized properly
	for (int i = 0; i < size && g->arr; ++i)
	{
		for (int j = 0; j < size; ++j)
		{
			for (int k = 0; k < size; ++k)
			{
				std::cout << "# of blue pixels in voxel "		//multiply by size^2 to i as size^2 elements
						  << (size*size * i + size * j + k)		//have been traversed and similarily for j but size^2 + size
						  << " : " << g->arr[i][j][k].bPixels << '\n';			 	
			}
		}
	}*/

	return 0;
}