#include <iostream>
#include <limits.h>
#include <math.h>
#include <vector>
using namespace std;
/*
	C++ program for
	Minimum weight cycle in an directed graph
*/
class AjlistNode
{
public:
	// Vertices node key
	int id;
	double weight;
	AjlistNode *next;
	AjlistNode(int id, double weight)
	{
		// Set value of node key
		this->id = id;
		this->weight = weight;
		this->next = nullptr;
	}
};

class Vertices
{
public:
	int data;
	AjlistNode *next;
	AjlistNode *last;
 	Vertices(int data)
    {
        this->data = data;
        this->next = nullptr;
        this->last = nullptr;
    }
    Vertices()
    {
        this->data = 0;
        this->next = nullptr;
        this->last = nullptr;
    }
};

class Graph
{
public:
	// Number of Vertices
	int size;
	double result;
	Vertices *node;
	vector<int> cycle; // vector to keep track of the cycle
	Graph(int size)
	{
		// Set value
		this->size = size;
		this->result = 0;
		this->node = new Vertices[size]; // create array on n vertices (Vertices)
		this->setData(); // set index of each vertice to data
	}
	// Set initial node value
	void setData()
	{
		if (this->size <= 0)
		{
			cout << "\nEmpty Graph" << endl;
		}
		else
		{
			for (int index = 0; index < this->size; index++)
			{
				// Set initial node value
				this->node[index].data = index;
			}
		}
	}
	void connection(int start, int last, double weight)
	{
		// Safe connection
		AjlistNode *edge = new AjlistNode(last, weight); // create new space to allocate adjency
		if (this->node[start].next == nullptr)
		{
			this->node[start].next = edge; // primera aresta 
		}
		else
		{
			// Add edge at the end
			this->node[start].last->next = edge;
		}
		// Get last edge
		this->node[start].last = edge; // per saber on es l'ubicació de l'últim vèrtex per fer la cadena.
	}
	//  Handling the request of adding new edge
	void addEdge(int start, int last, double weight)
	{
		if (start >= 0 && start < this->size &&
			last >= 0 && last < this->size)
		{
			this->connection(start, last, weight); // create new connection
		}
		else
		{
			// When invalid nodes
			cout << "\nHere Something Wrong" << endl;
		}
	}
	void printGraph()
	{
		if (this->size > 0)
		{
			// Print graph ajlist Node value
			for (int index = 0; index < this->size; ++index)
			{
				cout << "\nAdjacency list of vertex " << index << " :";
				AjlistNode *edge = this->node[index].next;
				while (edge != nullptr)
				{
					// Display graph node
					cout << "  " << this->node[edge->id].data << "[" << edge->weight << "]";
					// Visit to next edge
					edge = edge->next;
				}
			}
		}
	}
	void minimumCycle(int start, int last, bool visit[], double sum) // comprova tots els cicles possible i es queda el mes petit
	{
		cycle.push_back(this->node[start].data);
		if (start >= this->size || last >= this->size || start < 0 || last < 0 || this->size <= 0)
		{
			return;
		}
		if (visit[start] == true) // comprova si ja hem passat pel vertex. Vol dir que tanquem el cicle.
		{
			if (start == last) // condicio que torna al vertex inicial i tanca cicle 
			{
				if (cycle.size() > 2) {
					if (sum < -0.000025) {
						cout << "\033[1;35m\nMin cycle found: \033[1;32m";
						for (auto i = cycle.begin(); i != cycle.end(); ++i) 
						{
							cout << *i << " "; 
						}
						cout << "\033[0;34m  ->  " << sum << "  ->  \033[1;34m" << pow(10, -1 * sum) * 100 - 100 << "%\033[0;37m";
						if (sum < this->result) {
							cout << "\033[1;31m *** \033[0;34m";
						}
					}
					
					if (sum < this->result) {
						this->result = sum;
					}
				}


			}
			cycle.pop_back();
			return;
		}
		// Here modified  the value of visited node
		visit[start] = true;
		// This is used to iterate nodes edges
		AjlistNode *edge = this->node[start].next;
		while (edge != nullptr)
		{
			this->minimumCycle(
				edge->id,
				last,
				visit,
				sum + (edge->weight));
			// Visit to next edge
			edge = edge->next;
		}
		// Reset the value of visited node status
		cycle.pop_back();
		visit[start] = false;
	}
	void minWeightCycle()
	{
		if (this->size <= 0)
		{
			// Empty graph
			return;
		}
		// Auxiliary space which is used to store
		// information about visited node
		bool visit[this->size];
		// Set initial visited node status
		for (int i = 0; i < this->size; ++i) 
		{
			visit[i] = false;
		}
		this->result = 100000.0;
		for (int i = 0; i < 2; ++i) // 1 => this->size
		{
			// Check cycle of node i to i
			// Here initial cycle weight is zero
			this->minimumCycle(i, i, visit, 0);
		}
		if (this->result == 100000.0)
		{
			cout << "\nMin weight cycle : None " << endl;
		}
		else
		{
			cout << "\nMin weight cycle : " << this->result << endl;
			double profit = pow(10, -1 * this->result);
			cout << "Min weight cycle profit : " << profit << " " << profit * 100 - 100 << "%" << endl;
		}
	}
};

double graph[10][10] = 
{
{0.0, 0.9497578117580018, 5.845741406613988e-05, 0.0007873023871008376, 0.07390983000739099, 2.5568908207619536, 0.0034328870580157913, 1.088850174216028, 0.013031013812874642, 6.024096385542168, },
{1.0528, 0.0, 6.153755267614509e-05, 0.000828891854479746, 0.07782101167315175, 2.6917900403768504, 0.0036153289949385392, 1.146131805157593, 0.013719303059404582, 6.3411540900443875, },
{17106.25, 16245.78, 0.0, 13.471460710484838, 1264.8621300278269, 43744.53193350831, 58.77512636652168, 18628.912071535022, 222.9654403567447, 102986.61174047373, },
{1270.15, 1205.93, 0.074228, 0.0, 93.80863039399625, 3247.807729782397, 4.363001745200698, 1382.7433628318583, 16.550810989738498, 0.0, },
{13.52, 12.84, 0.0007904, 0.01064, 0.0, 0.0, 0.0464, 0.0, 0.0, 0.0, },
{0.391, 0.3714, 2.285e-05, 0.0003078, 0.0, 0.0, 0.001342, 0.0, 0.0, 0.0, },
{291.2, 276.5, 0.017013, 0.2291, 21.50537634408602, 744.6016381236038, 0.0, 316.9572107765452, 3.793626707132018, 1754.3859649122808, },
{0.9183, 0.8721, 5.367e-05, 0.0007227, 0.0, 0.0, 0.003154, 0.0, 0.0, 0.0, },
{76.73, 72.86, 0.004484, 0.06038, 0.0, 0.0, 0.2634, 0.0, 0.0, 0.0, },
{0.1659, 0.1575, 9.7e-06, 0.0, 0.0, 0.0, 0.0005698, 0.0, 0.0, 0.0, },
};




// {
// { 0.00000000, 0.96978956, 0.00006166, 0.00082962, 0.07723927, 2.97541046, 0.00366642, 1.15876985, 0.01759196, 5.31634474},
// { 1.03115153, 0.00000000, 0.00006359, 0.00085570, 0.07962015, 3.06562018, 0.00378459, 1.19680297, 0.01810417, 5.47035503},
// { 16217.95898438, 15724.68945313, 0.00000000, 13.45437050, 1252.48937988, 48285.85156250, 59.42956924, 18775.81640625, 285.51687622, 86281.27343750},
// { 1205.36755371, 1168.63110352, 0.07432529, 0.00000000, 93.03060913, 3583.45849609, 4.41944647, 1395.40075684, 21.18330765, 0.00000000},
// { 12.94678307, 12.55963516, 0.00079841, 0.01074915, 0.00000000, 0.00000000, 0.04747239, 0.00000000, 0.00000000, 0.00000000},
// { 0.33608809, 0.32619828, 0.00002071, 0.00027906, 0.00000000, 0.00000000, 0.00123089, 0.00000000, 0.00000000, 0.00000000},
// { 272.74591064, 264.22912598, 0.01682664, 0.22627269, 21.06487465, 812.42022705, 0.00000000, 316.26354980, 4.79956150, 1450.72607422},
// { 0.86298412, 0.83555943, 0.00005326, 0.00071664, 0.00000000, 0.00000000, 0.00316192, 0.00000000, 0.00000000, 0.00000000},
// { 56.84413910, 55.23590469, 0.00350242, 0.04720698, 0.00000000, 0.00000000, 0.20835236, 0.00000000, 0.00000000, 0.00000000},
// { 0.18809916, 0.18280350, 0.00001159, 0.00000000, 0.00000000, 0.00000000, 0.00068931, 0.00000000, 0.00000000, 0.00000000},
// };

int main()
{
	// 5 implies the number of nodes in graph
	Graph *g = new Graph(10);
	// Connect node with an edge
	// First and second parameter indicate node
	// Last parameter is indicate weight

	for (int i = 0; i < 10; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			double &prov = graph[i][j];
			if (prov > 0)
			{
				g->addEdge(i, j, -log10(prov));
			}
		}
	}
	// Print graph element
	g->printGraph();
	// Test
	g->minWeightCycle();
	return 0;
}