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

vector<pthread_t> leader_threads;
//semaphore to control sleep and wake up
sem_t stations[4];

//semaphore to access the logbook
sem_t logbook;
//semaphore for getting exclusive access to reader count 
sem_t mutex;
int reader_count = 0;
int logbook_total_completion = 0;
volatile bool stop_readers = false;
sem_t completion_count_sem;

void init_semaphore()
{
	for(int i=0;i<4;i++){
        sem_init(&stations[i],1,1);
    }
    sem_init(&completion_count_sem,1,1);
}



// Constants
#define MAX_WRITING_TIME 20   // Maximum time a agent can spend "writing"
#define WALKING_TO_PRINTER 10 // Maximum time taken to walk to the printer
#define SLEEP_MULTIPLIER 1000 // Multiplier to convert seconds to milliseconds

int N; // Number of agents
int y; //Logbook entry time 

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
// uses mutex lock to write output to avoid interleaving
void write_output(std::string output) {
  pthread_mutex_lock(&output_lock);
  std::cout << output;
  pthread_mutex_unlock(&output_lock);
}
enum agent_state { DOCUMENT_RECREATING, WAITING_FOR_LOGGING };

void* writer_activities(void* arg);

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
  agent(int id,int x) : id(id), state(DOCUMENT_RECREATING) {
    writing_time = x;
    completion_count = 0;
  }

  void notify_leader(int id){
    sem_wait(&completion_count_sem);
    this->completion_count++;
    sem_post(&completion_count_sem);
    if(this->completion_count==team_size){
      write_output("Unit "+to_string(team_number)+" has completed document recreation phase at time "+to_string(get_time())+"\n");
        //cout<<"leader of team "<<team_number<<" is going to write to logbook,his agent id: "<<this->id<<endl;
        pthread_t t;
        pthread_create(&t, NULL, writer_activities, (void*)this);
        leader_threads.push_back(t);
    }


    
  }
};

std::vector<agent> agents; // Vector to store all agents



/**
 * Simulate arriving at the printing station and log the time.
 */
void start_document_recreation(agent *agentt, int stationId) {
  
    write_output("Operative " + std::to_string(agentt->id) +
    " has started document recreation at time " +/*std::to_string(agentt->writing_time) +
    " ms at "*/   std::to_string(get_time()) + " at station "+to_string(stationId+1)+"\n");
    usleep(agentt->writing_time * SLEEP_MULTIPLIER); // Simulate document recreating time
    agentt->state = WAITING_FOR_LOGGING;

}

/**
 * Initialize agents and set the start time for the simulation.
 */
void initialize(int x) {
  for (int i = 1; i <= N; i++) {
    agents.emplace_back(agent(i,x));
  }

  // Initialize mutex lock
  pthread_mutex_init(&output_lock, NULL);
  sem_init(&logbook,1,1);
  sem_init(&mutex,1,1);

  start_time = std::chrono::high_resolution_clock::now(); // Reset start time
}

// const char* msg1 = "providing periodic operational progress reports to Thomas Shelby\n";
// const char* msg2 = "maintaining status updates on information board\n";
void *logbook_staff_member_activities(void * arg){
  int* staffid = (int*)arg;
    while(!stop_readers){
        usleep(get_random_number()%2000);
        write_output("[Time " + to_string(get_time()) + " ms] Intelligence Staff " + to_string(*staffid) + " is waiting to read the logbook..." + "\n");
        sem_wait(&mutex);
        reader_count = reader_count + 1;
        
        if(reader_count==1){
            sem_wait(&logbook);
        }
        sem_post(&mutex);
        write_output("Intelligence Staff "+to_string(*staffid)+" began reviewing logbook at time "+to_string(get_time())+". Operations completed = "+to_string(logbook_total_completion)+"\n");
        
        //cout << "[Time " << get_time() << " ms] ,staff " << *staffid << " has started reading the logbook." << endl;
        //cout<<staffmsg<<endl;
        usleep(1000); //simulating the logbook read time 
        sem_wait(&mutex);
        reader_count = reader_count - 1;
        if(reader_count==0){
            sem_post(&logbook);
        }
        write_output("[Time " + to_string(get_time())  + " ms] Intelligence Staff " + to_string(*staffid)  + " has finished reading the logbook." + "\n");
        sem_post(&mutex);
    }
    return NULL;
}

void* writer_activities(void* arg) {
  agent* leader = (agent*) arg;

  write_output("[Time " + to_string(get_time()) + " ms] " +
               "Leader of Team " + to_string(leader->team_number) +
               " (Agent " + to_string(leader->id) + ") is waiting to write to the logbook...\n");

  sem_wait(&logbook);  

  write_output("[Time " + to_string(get_time()) + " ms] " +
               "Leader of Team " + to_string(leader->team_number) +
               " (Agent " + to_string(leader->id) + ") is writing to the logbook...\n");

  logbook_total_completion += 1;  

  usleep(y*SLEEP_MULTIPLIER); //y=Logbook entry time  

  // write_output("[Time " + to_string(get_time()) + " ms] " +
  //              "Leader of Team " + to_string(leader->team_number) +
  //              " (Agent " + to_string(leader->id) + ") has completed writing to the logbook. " +
  //              "Operations completed = " + to_string(logbook_total_completion) + "\n");
  write_output("Unit "+to_string(leader->team_number) +" has completed intelligence distribution at time "+to_string(get_time())+
                ",Operations completed = " + to_string(logbook_total_completion) +"\n");

  sem_post(&logbook);  

  return NULL;
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

  write_output("Operative " + std::to_string(agentt->id) +
               " has arrived at typewriting station " + to_string(stationId+1)  +
               " at time " + std::to_string(get_time()) + "\n");  

 sem_wait(&stations[stationId]);

 start_document_recreation(agentt,stationId);  // agent reaches the document recreation station
 write_output("Operative " + std::to_string(agentt->id) +
               " has completed document recreation at time " + std::to_string(get_time()) + " at station "+to_string(stationId+1)+"\n");

  // usleep((get_random_number() % WALKING_TO_PRINTER + 1) *
  //        SLEEP_MULTIPLIER); // Simulate walking to printer


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

  pthread_t logbook_staffs[2];

  int* staffid1 = new int(1);
  int *staffid2= new int(2);



  init_semaphore();



  int remainingagents = N;
  bool started[N];

  int M;
  cin>>M;

  int x;
  cin>>x;
  initialize(x); // Initialize agents and mutex lock
  cin>>y;

  init_teams(agents,N,M);


  pthread_create(&logbook_staffs[0],NULL,logbook_staff_member_activities,(void*)staffid1);
 
  pthread_create(&logbook_staffs[1],NULL,logbook_staff_member_activities,(void*)staffid2);

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
   stop_readers = true;
  for (pthread_t t : leader_threads) {
        pthread_join(t, NULL);
    }
//   pthread_join(logbook_staffs[1],NULL);


  // Restore std::cin and cout to their original states (console)
  std::cin.rdbuf(cinBuffer);
  std::cout.rdbuf(coutBuffer);

  return 0;
}

