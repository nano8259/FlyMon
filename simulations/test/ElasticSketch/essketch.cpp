#include "DataTrace.h"
#include "HowLog/HowLog.h"
#include <unordered_map>
#include <string>
#include <cmath>
#include "TracePacket.h"
#include "ES_Sketch.h"
#include "ES_Params.h"


using namespace std;

// 修改了bucket, 记得改回来
    
#define TOT_MEM_IN_BYTES 786432
#define HEAVY_MEM (TOT_MEM_IN_BYTES/4)
#define BUCKET_NUM (HEAVY_MEM / (COUNTER_PER_BUCKET * (4+8)) )

double get_real_entropy(unordered_map<string, int>& flow_map){
    unordered_map<int, int> size_map;
    double m = 0;
    for(auto& flow : flow_map){
        m += flow.second;
    }
    double entropy = 0;
    for(auto& flow : flow_map){
        double p = flow.second/m;
        entropy += p*log2(p);
    }
    return -1*entropy;
}

int main(){
    LOG_LEVEL = L_INFO;
    DataTrace trace;
    // trace.LoadFromFile("../data/WIDE/one_sec_15.dat");
    // trace.LoadFromFile("../data/WIDE/five_sec_0.dat");
    trace.LoadFromFile("../data/WIDE/fifteen1.dat");
    // trace.LoadFromFile("../data/WIDE/thirty_sec_0.dat");
    // trace.LoadFromFile("../data/WIDE/sixty_sec_0.dat");
    int packet_count = trace.size();
    int threashold = 10000;

    const int key_len = 8;
	StreamAlgorithm *elastic = new ElasticSketch<BUCKET_NUM, TOT_MEM_IN_BYTES>(key_len); // key_len
    
    HOW_LOG(L_DEBUG, "Test elastic_sketch_sw, memory allocated : %d B, flow_key = %d", TOT_MEM_IN_BYTES, key_len);  

	unordered_map<string, int> Real_Freq;

    for (auto it = trace.begin(); it!= trace.end(); ++it){
        string str;
        if (key_len == 4){
            elastic->insert((*it)->getSrcBytes());
            str = string((const char*)((*it)->getSrcBytes()), 4);
        }else{
            elastic->insert((*it)->getFlowKey_IPPair());
            str = string((const char*)((*it)->getFlowKey_IPPair()), 8);         
        }
		Real_Freq[str]++;
    }

    vector< pair<string, int> > real_heavy_hitters;
    for (auto item : Real_Freq){
        if (item.second >= threashold)
            real_heavy_hitters.push_back(make_pair(item.first, item.second));
    }
    int real_cardinality = Real_Freq.size();
    int real_entropy= get_real_entropy(Real_Freq);

    double temp_relation_error_sum = 0;
    int key_num = Real_Freq.size();
    for (auto item : Real_Freq){
        string key = item.first;
 		int estimate = elastic -> query((uint8_t *)key.c_str());
		double relative_error = abs(item.second - estimate) / (double)item.second;
		temp_relation_error_sum += relative_error;      
        // HOW_LOG(L_INFO, "Flow %s, real=%d, estimate=%d", TracePacket::bytes_to_ip_str((uint8_t *)key.c_str()).c_str(), item.second, estimate); 
    }
    HOW_LOG(L_DEBUG, "Total %d packets, %d flows, ARE = %f", trace.size(), 
                     Real_Freq.size(), 
                     temp_relation_error_sum/Real_Freq.size());

    vector< pair<string, int> > est_heavy_hitters;
    elastic->get_heavy_hitters(threashold, est_heavy_hitters);
    int estimate_right = 0;
    for(int i = 0; i < (int)est_heavy_hitters.size(); ++i)
    {
        string key = est_heavy_hitters[i].first;
        // HOW_LOG(L_DEBUG, "<%s, %d>", srcIP.c_str(), est_heavy_hitters[i].second); 
        for(int j = 0; j < (int)real_heavy_hitters.size(); ++j)
        {
            if(real_heavy_hitters[j].first == key){
                estimate_right += 1;
                break;
            }
        }
    }
    double precision =  (double)estimate_right / (double)est_heavy_hitters.size();
    double recall = (double)estimate_right / (double)real_heavy_hitters.size();
    double f1 = (2 * precision * recall) / (precision + recall);
    HOW_LOG(L_DEBUG, "Real Heavyhitter = %d, ES Heavyhitter = %d, PR = %.2f, RR = %.2f, F1 Score = %.2f", real_heavy_hitters.size(), est_heavy_hitters.size(), precision, recall, f1); 

    int es_cardinality = elastic->get_cardinality();
    double re = abs(es_cardinality - real_cardinality) / (double)real_cardinality;
    HOW_LOG(L_DEBUG, "Real Cardinality = %d, ES Cardinality = %d, RE = %.2f", real_cardinality, es_cardinality, re); 


    int es_entropy = elastic->get_entropy();
    re = abs(real_entropy - es_entropy) / (double)real_entropy;
    HOW_LOG(L_DEBUG, "Real Entropy = %d, ES Entropy = %d, RE = %.2f", real_entropy, es_entropy, re); 

}


