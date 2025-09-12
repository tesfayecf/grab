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
				if (cycle.size() > 3) {
					// if (sum < -0.000025) {
					// 	cout << "\033[1;35m\nMin cycle found: \033[1;32m";
					// 	for (auto i = cycle.begin(); i != cycle.end(); ++i) 
					// 	{
					// 		cout << *i << " "; 
					// 	}
					// 	cout << "\033[0;34m  ->  " << sum << "  ->  \033[1;34m" << pow(10, -1 * sum) * 100 - 100 << "%\033[0;37m";
					// 	if (sum < this->result) {
					// 		cout << "\033[1;31m *** \033[0;34m";
					// 	}
					// }
					
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
		for (int i = 0; i < this->size; ++i) // 1 => this->size
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