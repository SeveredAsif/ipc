

#include <chrono>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <random>
#include <unistd.h>
#include<semaphore.h>
#include <vector>
#include<string>

using namespace std;

//semaphore to control sleep and wake up
sem_t stations[4];


void init_semaphore()
{
	for(int i=0;i<4;i++){
        sem_init(&stations[i],1,1);
    }
}



// Constants
#define MAX_WRITING_TIME 20   // Maximum time a agent can spend "writing"
#define WALKING_TO_PRINTER 10 // Maximum time taken to walk to the printer
#define SLEEP_MULTIPLIER 1000 // Multiplier to convert seconds to milliseconds

int N; // Number of agents

// Mutex lock for output to file for avoiding interleaving
pthread_mutex_t output_lock;

// Timing functions
auto start_time = std::chrono::high_resolution_clock::now();

/**
 * Get the elapsed time in milliseconds since the start of the simulation.
 * @return The elapsed time in milliseconds.
 */
long long get_time() {
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  long long elapsed_time_ms = duration.count();
  return elapsed_time_ms;
}

// Function to generate a Poisson-distributed random number
int get_random_number() {
  std::random_device rd;
  std::mt19937 generator(rd());

  // Lambda value for the Poisson distribution
  double lambda = 10000.234;
  std::poisson_distribution<int> poissonDist(lambda);
  return poissonDist(generator);
}

enum agent_state { WRITING_REPORT, WAITING_FOR_PRINTING };

/**
 * Class representing a agent in the simulation.
 */
class agent {
public:
  int id;              // Unique ID for each agent
  int writing_time;    // Time agent spends "writing"
  agent_state state; // Current state of the agent (writing or waiting)
  agent* leader;
  int team_number;
  int completion_count;
  int team_size;

  /**
   * Constructor to initialize a agent with a unique ID and random writing
   * time.
   * @param id agent's ID.
   */
  agent(int id) : id(id), state(WRITING_REPORT) {
    writing_time = get_random_number() % MAX_WRITING_TIME + 1;
    completion_count = 0;
  }

  void notify_leader(int id){
    this->completion_count++;
    if(this->completion_count==team_size){
        cout<<"leader of team "<<team_number<<" is going to write to logbook,his agent id: "<<this->id<<endl;
    }
  }
};

std::vector<agent> agents; // Vector to store all agents

// uses mutex lock to write output to avoid interleaving
void write_output(std::string output) {
  pthread_mutex_lock(&output_lock);
  std::cout << output;
  pthread_mutex_unlock(&output_lock);
}

/**
 * Simulate arriving at the printing station and log the time.
 */
void start_printing(agent *agent) {
  agent->state = WAITING_FOR_PRINTING;

  write_output("agent " + std::to_string(agent->id) +
               " has arrived at the print station at " +
               std::to_string(get_time()) + " ms\n");
}

/**
 * Initialize agents and set the start time for the simulation.
 */
void initialize() {
  for (int i = 1; i <= N; i++) {
    agents.emplace_back(agent(i));
  }

  // Initialize mutex lock
  pthread_mutex_init(&output_lock, NULL);

  start_time = std::chrono::high_resolution_clock::now(); // Reset start time
}

/**
 * Thread function for agent activities.
 * Simulates the agent's report writing and reaching the print station.
 * @param arg Pointer to a agent object.
 */
void *agent_activities(void *arg) {
  agent *agentt = (agent*) arg;

  //take mutex of the desired station 
  //desired station = id % 4 + 1
  int stationId = agentt->id % 4;

  write_output("agent " + std::to_string(agentt->id) +
               " started waiting at station " + to_string(stationId+1)  +
               " at " + std::to_string(get_time()) + " ms\n");  

 sem_wait(&stations[stationId]);

  write_output("agent " + std::to_string(agentt->id) +
               " started writing for " + std::to_string(agentt->writing_time) +
               " ms at " + std::to_string(get_time()) + " ms at station "+to_string(stationId+1)+"\n");
  usleep(agentt->writing_time * SLEEP_MULTIPLIER); // Simulate writing time
  write_output("agent " + std::to_string(agentt->id) +
               " finished writing at " + std::to_string(get_time()) + " ms at station "+to_string(stationId+1)+"\n");

  usleep((get_random_number() % WALKING_TO_PRINTER + 1) *
         SLEEP_MULTIPLIER); // Simulate walking to printer
  start_printing(agentt);  // agent reaches the printing station

  agentt->leader->notify_leader(agentt->id);
  //cout<<"team of agent"<<agentt->id<<" is team: "<<agentt->team_number<<endl;
  sem_post(&stations[stationId]);
  return NULL;
}

void init_teams(vector<agent> &agents,int N,int M)
{
  //assign each agent to a team. 
  //assign a team leader to each team. the highest id will be set as team leader. 
  int team_number = N/M + 1;
  agent* leader;
  for(int i=N-1;i>=0;i--){
    if(i%M==M-1){
        team_number--;
        leader = &agents[i];
        leader->team_size = M;
        cout<<"agent: "<<leader->id<<" assigned as the leader of team "<<team_number<<endl;
    }
    //setting the leader;
    agents[i].leader = leader;
    agents[i].team_number = team_number;
    cout<<"agent: "<<agents[i].id<<" assigned to team "<<team_number<<" and his leader is agent "<<leader->id<<endl;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cout << "Usage: ./a.out <input_file> <output_file>" << std::endl;
    return 0;
  }

  // File handling for input and output redirection
  std::ifstream inputFile(argv[1]);
  std::streambuf *cinBuffer = std::cin.rdbuf(); // Save original std::cin buffer
  std::cin.rdbuf(inputFile.rdbuf()); // Redirect std::cin to input file

  std::ofstream outputFile(argv[2]);
  std::streambuf *coutBuffer = std::cout.rdbuf(); // Save original cout buffer
  std::cout.rdbuf(outputFile.rdbuf()); // Redirect cout to output file

  // Read number of agents from input file
  std::cin >> N;

  pthread_t agent_threads[N]; // Array to hold agent threads

//   for(int i=0;i<N;i++){
//     agent new_agent(i);
//     agents.push_back(new_agent);
//   }


  init_semaphore();

  initialize(); // Initialize agents and mutex lock

  int remainingagents = N;
  bool started[N];

  int M;
  cin>>M;
  init_teams(agents,N,M);

  // start agent threads randomly
  while (remainingagents) {
    int randomagent = get_random_number() % N;
    if (!started[randomagent]) {
      started[randomagent] = true;
      pthread_create(&agent_threads[randomagent], NULL, agent_activities,
                     &agents[randomagent]);
      remainingagents--;
      usleep(1000); // sleep for 1 ms
      if (get_time() >
          7000) { // if more than 7 seconds is passed, initialize the rest
        break;
      }
    }
  }

  

  // after waiting for 7(our choice) secs, start the remaining agent threadss
  for (int i = 0; i < N; i++) {
    if (!started[i]) {
      pthread_create(&agent_threads[i], NULL, agent_activities,
                     &agents[i]);
    }
  }

  // Wait for all agent threads to finish
  for (int i = 0; i < N; i++) {
    pthread_join(agent_threads[i], NULL);
  }

  // Restore std::cin and cout to their original states (console)
  std::cin.rdbuf(cinBuffer);
  std::cout.rdbuf(coutBuffer);

  return 0;
}

/*
  Prepared by: Nafis Tahmid (1905002), Date: 10 November 2024
*/