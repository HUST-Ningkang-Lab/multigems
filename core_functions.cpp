/*
 * core_functions.cpp
 *
 *  Created on: Feb 4, 2015
 *      Author: xinping
 */
#include <ctime>
#include <cmath>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <queue>
#include <array>

#include <omp.h>

#include "core_functions.h"

using namespace std;

map<unsigned int, shared_ptr<Multi_Seq_Obj>> pos_samples_map;
Parameters params;

int printhelp(){
    // Width 12345678911234567892123456789312345678941234567895123456789612345678971234567898
    cout << "MultiGeMS 1.0\n\n";
    cout << "Multi-sample Genotype Model Selection (MultiGeMS) is a multiple sample single\n";
    cout << "nucleotide variant (SNV) caller that works with alignment files of\n";
    cout << "high-throughput sequencing (HTS) data. MultiGeMS calls SNVs based on a\n";
    cout << "statistical model selection procedure and accounts for enzymatic substitution\n";
    cout << "sequencing errors.\n\n";
    cout << "Compile\n\n";
    cout << "MultiGeMS can be compiled using a GCC compiler with C++11 support, by running:\n";
    cout << "$ make\n\n";
    cout << "Input\n\n";
    cout << "MultiGeMS accepts a text file, listing on seperate lines, paths of SAMtools\n";
    cout << "pileup format files. To convert a SAM/BAM alignment file into the pileup\n";
    cout << "format, users can use the SAMtools mpileup procedure with option -s.\n\n";
    cout << "Filter\n\n";
    cout << "Alignment file reads with undesirable characteristics can be filtered before\n";
    cout << "running MultiGeMS. For an explanation why filtering may be desirable and for a\n";
    cout << "brief tutorial on how to filter SAM/BAM alignment files using SAMtools view,\n";
    cout << "please see the PDF document entitled 'Filtering Alignment Files' available at\n";
    cout << "https://github.com/cui-lab/multigems.\n\n";
    cout << "Usage\n\n";
    cout << "multigems -i pileuplist.txt -o multigems.out [OPTIONS]\n\n";
    cout << "Options\n\n";
    cout << "-b INT   minimum base-calling quality score considered, default is 17\n";
    cout << "-m INT   minimum mapping quality score considered, default is 20\n";
    cout << "-s FLOAT maximum likelihood computing steps float value, smaller is slower yet\n";
    cout << "         more precise, default is 0.01\n";
    cout << "-e FLOAT EM algorithm convergence threshold, smaller is slower yet more\n";
    cout << "         precise, default is 0.001\n";
    cout << "-M INT   maximum number of bases to be considered from each sample of each\n";
    cout << "         site, 0 indicates unbounded, default is 255\n";
    cout << "-f FLOAT lFDR SNV threshold, between 0 and 1, default is 0.1\n";
    cout << "-C INT   number of sites to be analyzed per analysis cycle, smaller is slower\n";
    cout << "         in general and uses less RAM, default is 200\n";
    cout << "-t INT   number of threads, can be used in conjunction with -C option for\n";
    cout << "         multi-thread efficiency, default is 1\n";
    cout << "-n FLOAT site non-reference allele proportion filter (sites where all samples\n";
    cout << "         have a non-reference proportion less than this filter are not\n";
    cout << "         analyzed), smaller is slower and more susceptible to false positive\n";
    cout << "         SNV calls, higher is faster and more susceptible to false negative SNV\n";
    cout << "         calls, between 0 and 1, default is 0.2\n";
    cout << "-l FLOAT sample deletion placeholder proportion filter (samples with pileup\n";
    cout << "         deletion placeholder proportions greater than filter are not\n";
    cout << "         analyzed), between 0 and 1, default is 0.05\n\n";
    cout << "Output\n\n";
    cout << "The MultiGeMS output is similar to that of the Variant Call Format (VCF) file\n";
    cout << "format. Meta-information lines are provided at the beginning of each output.\n";
    cout << "Only sites less than the user-selected lFDR SNV threshold are output and are\n";
    cout << "considered SNV calls. An lFDR output of '-1' indicates that the site was\n";
    cout << "analyzed by MultiGeMS, but not enough information was retained post-filtering\n";
    cout << "to estimate the lFDR value for the site.\n\n";
    cout << "Contact\n\n";
    cout << "Xinping Cui\n";
    cout << "xinping.cui@ucr.edu\n";
    cout << "https://sites.google.com/a/bioinformatics.ucr.edu/xinping-cui/\n\n";
    // Width 12345678911234567892123456789312345678941234567895123456789612345678971234567898
    exit(0);
}

int Get_Name_List(const string &listname, vector<string> &infilename)
{
    ifstream infile(listname, ifstream::in);
    if (!infile)
    {
    	cerr <<"Open list error : " << listname << endl;
    	exit(0);
    }

    string buffer;
    int count = 0;

    while(getline(infile, buffer))
    {
    	if (!buffer.empty())
    	{
    		infilename.push_back(buffer);
    		count++;
    	}
    }

    infile.close();
    return count;
}

int calculate_values(double end)
{
	int count = 0;
	for (map<unsigned int, shared_ptr<Multi_Seq_Obj>>::iterator it = pos_samples_map.begin(); it != pos_samples_map.end(); it++)
	{
		count++;
		it->second.get()->Calc_EM(end, params.step, params.eps);
		it->second.get()->Calc_W(2, 200);
	}
	return count;
}

int calculate_values_omp(double end, int thread)
{
	omp_set_num_threads(thread);
	unsigned int pos_list[pos_samples_map.size()];
	int count = 0;
	for (map<unsigned int, shared_ptr<Multi_Seq_Obj>>::iterator it = pos_samples_map.begin(); it != pos_samples_map.end(); it++)
	{
		pos_list[count] = it->first;
		count++;
	}

	#pragma omp parallel for schedule(dynamic)
	for (int i = 0; i < count; i++)
	{
		if (params.debug) {
			cout << pos_list[i] << endl << endl;
		}
		pos_samples_map[pos_list[i]].get()->Calc_EM(end, params.step, params.eps);
		pos_samples_map[pos_list[i]].get()->Calc_W(2, 200);
	}

	return count;
}

int String_Split(const string &buffer, array<string, 7> &obj, int n)
{
	stringstream strin(buffer);
	for (int i = 0; i < n; i++)
		strin >> obj[i];
	return 0;
}

void calculate_preprocess(const vector<string> &infilename, string &outfilename)
{
	ifstream* ifstream_array = new ifstream[params.sample_count];
	vector<queue<string>> buffer_queue(params.sample_count);

	//open files
	for (int i = 0; i < params.sample_count; i++)
	{
		ifstream_array[i].open(infilename[i], ios::in);
		if (!ifstream_array[i])
		{
			cerr <<"Open infile error : " << infilename[i] << endl;
			exit(0);
		}
	}

	ofstream output_file(outfilename, ios::out);
	if (!output_file)
	{
		cerr << "Open outfile error : " << outfilename << endl;
		exit(0);
	}

	//Let us circle
	core_calculate(ifstream_array, buffer_queue, output_file);

	//close files
	for (int i = 0; i < params.sample_count; i++)
		ifstream_array[i].close();
	output_file.close();

	cout << "GeMS finished, please check the results at " << outfilename << endl;
}

void output_header(ofstream &out)
{
	time_t rawtime;
	struct tm * timeinfo;
	char buffer [9];
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer, 9, "%Y%m%d", timeinfo);
	out << "##fileformat=VCFv4.2" << endl;
	out << "##fileDate=" << buffer << endl;
	out << "##source=multiGeMSV2.0" << endl;
	out << "##reference=UNKNOWN" << endl;
	out << "##contig=<ID=NA,length=unknown,assembly=NA,md5=NA,species=NA,taxonomy=unknown>" << endl;
	out << "##phasing=partial" << endl;
	out << "##INFO=<ID=NA,Number=NA,Type=NA,Description=\"INFO is not applicable in this version.\"" << endl;
	out << "##FILTER=<ID=q10,Description=\"Quality below 10\"" << endl;
	out << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tSample_Number\tMu_0\tMu_1\tRR\tRN\tNN\tlFDR" << endl;
}

void core_calculate(ifstream* ifstream_array, vector<queue<string>> &buffer_queue, ofstream &output_file)
{
	srand((int)time(NULL));
	vector<unsigned int> count_vector(params.sample_count, 0);
	cout << "Sample number : " << params.sample_count << endl;
	vector<bool> queue_flag(params.sample_count, true);
	int true_flag_count = params.sample_count;

	output_header(output_file);

	int circle_count = 0;
	while (true_flag_count > 0)
	{
		for (int i = 0; i < params.sample_count; i++)
		{
			if (queue_flag[i])
			{
				new_read(ifstream_array[i], buffer_queue[i], buffer_queue[i].size());
				if (buffer_queue[i].size() < params.one_circle_limit)
				{
					queue_flag[i] = false;
					true_flag_count--;
				}
			}
		}

		long checkin_limit = MAX_NUM;

		if (true_flag_count != 0)
			checkin_limit = min_last_element(buffer_queue);
		cout << "checkin_limit = " << checkin_limit << endl;
		for (int i = 0; i < params.sample_count; i++)
		{
			if (buffer_queue[i].size() > 0)
			{
				//cout << i << endl;
				data_checkin(buffer_queue[i], count_vector, checkin_limit, i);
			}

		}

		cout << endl << ++circle_count << " Loading finished" << endl;
		cout << pos_samples_map.size() << " Pos loaded" << endl;
		cout << position_reduce() << " Pos Removed" << endl;
		cout << pos_samples_map.size() << " Pos remained"<< endl;

		//calculate_values(params.end_condition);
		calculate_values_omp(params.end_condition, params.thread);

		cout << "Writing results.." << endl;

		output_values(output_file);
	}
}

int new_read(ifstream &in, queue<string> &buffer, int len)
{
	string line;
	while((len < params.one_circle_limit) && (getline(in, line)))
	{
		//stringstream ss(line);
		//string temp;
		//ss >> temp;
		//ss >> temp;
		//if (stoi(temp) >= params.start_position) {
		buffer.push(line);
		len++;
		//}
	}
	return len;
}

long min_last_element(vector<queue<string>> &buffer_queue)
{
	long min = MAX_NUM;
	vector<queue<string>>::iterator it = buffer_queue.begin();
	while(it != buffer_queue.end())
	{
		if (!it->empty())
		{
			string line = it->back();
			//cout << line << endl;
			array<string, 7> obj;
			String_Split(line, obj, 7);
			long pos = stol(obj[1]);
			if (pos < min)
				min = pos;
		}
		it++;
	}
	return min;
}

void position_add(vector<unsigned int> &count_vector, shared_ptr<Seq_Obj> &seq_obj, int sample)
{
	unsigned int pos = seq_obj.get()->Get_Pos();

	if (pos_samples_map.count(pos) == 0)
	{
		pos_samples_map[pos].reset(new Multi_Seq_Obj(params.sample_count, params.type));
	}
	pos_samples_map[pos].get()->Insert(seq_obj, sample);
	count_vector[sample]++;
}

void data_checkin(queue<string> &buffer, vector<unsigned int> &count_vector, long checkin_limit, int sample)
{
	while (!buffer.empty())
	{
		//cout << buffer.front() << endl;
		array<string, 7> obj;
		String_Split(buffer.front(), obj, 7);

		if (stol(obj[1]) > checkin_limit)
			break;
		//Check if qual_bq_length = qual_mq_length
		if (obj[5].size() != obj[6].size()) cout <<"Qual Seq error : " << buffer.front()<< endl;
		shared_ptr<Seq_Obj> seq_obj(new Seq_Obj(obj, params.type, params.step, params.end_condition));

		if (seq_obj.get()->Get_Ref() == "N") {
			buffer.pop();
			continue;
		}
		if (seq_obj.get()->Seq_Init_Filter() == 1)
			cout << "this is the one with plus error : " << buffer.front() << endl;
		if ((seq_obj.get()->Get_Ratio_nchar() >= params.ratio_nchar)&&(seq_obj.get()->Get_Ratio_del() < params.ratio_del))
		{
			unsigned int temp_pos = seq_obj.get()->Get_Pos();
			seq_obj.get()->Seq_Qual_Filter(params.bp, params.mp);
			seq_obj.get()->Seq_Max_Filter(params.max_count);
			position_add(count_vector, seq_obj, sample);
			if (pos_samples_map.find(temp_pos) != pos_samples_map.end())
				pos_samples_map[temp_pos].get()->Enable();
		}
		else
		{
			seq_obj.get()->Seq_Qual_Filter(params.bp, params.mp);
			seq_obj.get()->Seq_Max_Filter(params.max_count);
			position_add(count_vector, seq_obj, sample);
		}
		buffer.pop();
	}
}

unsigned int position_reduce()
{
	unsigned int reduce_count = 0;

	//Remove disabled
	map<unsigned int, shared_ptr<Multi_Seq_Obj>>::iterator it = pos_samples_map.begin();
	while (it != pos_samples_map.end())
	{
		if (!it->second.get()->Get_Is_Qual())
		{
			map<unsigned int, shared_ptr<Multi_Seq_Obj>>::iterator toErase = it;
			it++;
			reduce_count++;
			pos_samples_map.erase(toErase);
		}
		else
		{
			it++;
		}
	}

	return reduce_count;
}

void output_values(ofstream &out)
{
	char Consensus_letter[11] = "ACGTMRWSYK";

	unsigned int Ti = 0;
	unsigned int Tv = 0;

	map<unsigned int, shared_ptr<Multi_Seq_Obj>>::iterator it = pos_samples_map.begin();

	while (it != pos_samples_map.end())
	{
		unsigned int pos = it->first;

		//Let the filter effect
		if (pos_samples_map.at(pos).get()->Get_Is_Qual()
			&& (pos_samples_map.at(pos).get()->Get_W() < params.result_filter))
		{
			out << pos_samples_map.at(pos).get()->Get_Chrom() << "\t";
			out << pos << "\tNA\t" << pos_samples_map.at(pos).get()->Get_Ref() << "\tNA\t";

			if ((pos_samples_map.at(pos).get()->Get_Sample_Count() >= 1)
				&& (pos_samples_map.at(pos).get()->Get_W() >= 0))
			{
				if (pos_samples_map.at(pos).get()->Get_W() < pow(10, -100))
				{
					out << "\t" << 999.999 << "\tPASS\t";
				}
				else
				{
					out << "\t" << -10 * log(pos_samples_map.at(pos).get()->Get_W()) << "\tPASS\t";
				}
			}
			else
				out << "\tNA\tNA\t";
			for (unsigned int i = 0; i < params.sample_count; i++)
			{
				if (pos_samples_map.at(pos).get()->Get_Is_Sample(i)) {
					out << i << ":" << pos_samples_map.at(pos).get()->Get_E_Value_Max(i) + 1 << ",";
				} else
					out << i << ":NA,";
			}
			out << "P0:" << pos_samples_map.at(pos).get()->Get_P(0)
			    << ",P1:" << pos_samples_map.at(pos).get()->Get_P(1);

			out << "\t" << "Sample Number: " << pos_samples_map.at(pos).get()->Get_Sample_Count();
			out << "\t" << pos_samples_map.at(pos).get()->Get_P(0);
			out << "\t" << pos_samples_map.at(pos).get()->Get_P(1);
			out << "\t" << pos_samples_map.at(pos).get()->Get_Value(0);
			out << "\t" << pos_samples_map.at(pos).get()->Get_Value(1);
			out << "\t" << pos_samples_map.at(pos).get()->Get_Value(2);
			out << "\t" << pos_samples_map.at(pos).get()->Get_W();
			out << endl;


		}
		//Ext info
		int max_value = pos_samples_map.at(pos).get()->Get_Value_Max();

		if ((pos_samples_map.at(pos).get()->Get_P(0) <= params.p_snp)
				&& ((pos_samples_map.at(pos).get()->Get_Ref())[0]
						!= Consensus_letter[max_value])) //Count Ti/Tv
		{
			if ((((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'A')
					&& (Consensus_letter[max_value] == 'G'))
					|| (((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'G')
							&& (Consensus_letter[max_value] == 'A'))
					|| (((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'C')
							&& (Consensus_letter[max_value] == 'T'))
					|| (((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'T')
							&& (Consensus_letter[max_value] == 'C')))

				Ti++;

			else if ((((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'A')
					&& (Consensus_letter[max_value] == 'C'))
					|| (((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'C')
							&& (Consensus_letter[max_value] == 'A'))
					|| (((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'A')
							&& (Consensus_letter[max_value] == 'T'))
					|| (((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'T')
							&& (Consensus_letter[max_value] == 'A'))
					|| (((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'C')
							&& (Consensus_letter[max_value] == 'G'))
					|| (((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'G')
							&& (Consensus_letter[max_value] == 'C'))
					|| (((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'G')
							&& (Consensus_letter[max_value] == 'T'))
					|| (((pos_samples_map.at(pos).get()->Get_Ref())[0] == 'T')
							&& (Consensus_letter[max_value] == 'G')))

				Tv++;
		}
		it++;
	}
	pos_samples_map.clear();
}


void test()
{
	srand((int)time(NULL));
	params.debug = true;
	params.sample_count = 10;
	queue<string> buffer_queue;
	vector<unsigned int> count_vector(params.sample_count, 0);
	/*buffer_queue.push("NA12877_S1.ch18.pile:chr18	15208082	c	65	,,,.g.,,GG,G.....,....,.,,,,,.g.g,G,,ggg,,.,g.G.ggG,gg.g.g,.,g,g.	<CF6B>HJ;)JD:=IGJJGJIGFGDC7DDEAEDDJBD<DDDDJDAIIE?DIBB<?D?BBFDDB@+	]]]]5]]]>>]>]]]>>]]>]]]>]]]]]]>]>]>>]>>>]]]]>]>]>>>]>>]>]>]]]:]>]");
	buffer_queue.push("NA12878_S1.ch18.pile:chr18	15208082	c	72	,,..,gG..,,.,..G,,..,.,,.g.g,ggg.GgGgGgG,gg,ggGGggGg.ggGggG,.ggg,.g....g	FBDDJJDHABFGGFGIIIAJFJBFGDJ(DDDDEJDGBJDJDDDDDBJE1BJDD?DHBDDDFBADDFD@C@@<	Z]>]]>>]]]]>]]>>]]]]]>]]]>>>]>>>]>>>>>>>]>>]FF>>>>]>]>F>>>>]>5:F]>F>]]]F");
	buffer_queue.push("NA12879_S1.ch18.pile:chr18	15208082	c	63	,,,.,G.G..,..,,....,,.,.G,g,.ggG,Gg,G,gGGGGg.gggGgggg.,.g,.,ggg	CFH7J(AD;HI;IIJGF@IJIG?IID?DGDDGDIBDIDDIIIIBEDBBHDBDD4@DD<F8DDB	]]]]]>]>]>]>]]]]]]>]]>Z]>]>]]>>>]>>]>]>>>>>>>>>>>>>>>>]]>]]]>>>");
	buffer_queue.push("NA12880_S1.ch18.pile:chr18	15208082	c	72	,.,,,.,,,,,,.,..,,,...G,.,,......,,g.GgGg,GGGG,.Ggg,gGG,Gg.g.ggggggg...,	F:FHCDHGGJGJHIJGGJIJIG=HB=HIJGIIJD@9FIDIDDJIHI8IJAB?BIEDIBHDHABDB(BDD@@B	W]]]]]>]]]]]]]]]]]]]]]>]]]]]]>:]]Z]:]>F>>]>>>>]>>>>Z>>>]>>>>>>>>>>>>]>]]");
	buffer_queue.push("NA12882_2_S1.ch18.pile:chr18	15208082	c	68	,.,,..,.,,....,,.,..,,,,,,,.,G.G.g,.G,,G,gGgG.,gGG,..GG.GGGg,,g,gg.g	FCFHC3HHIJDIGHJI@IH?FFFDDDDGDI4JIB?GEDDIDDGDGIDBJIDJGJ>HGHHDDDDDDD@@	]]]]]]]]]]]]]]]X>]]]]]:]]]]>]>]>]>]]>]X>]>>>>]]>>>]]]>>]>>>>]]>]F>>>");
	buffer_queue.push("NA12883_S1.ch18.pile:chr18	15208082	c	91	G,T..,,,.G.G,g,G,.....,,.,.,,,,.,.Gg.,,gg.,g.gg,GggGG..,gg.gGGgGGGGg,gG..g.ggg,gg.gg..gg,.g	:@(CCHHF)>@@EIGHJGAEEIJIJH*HDDBJDECDGDDDDHDDFDDBGB+FDJJDDDIDJIBIIJIBDBHFFDFB?DBCDFBDBCDDBF2	>]>]]]]]]>>>]F]>]]]]]]]]]]>]]]]]]]>>]]]>>>]:]>>]>5>>>>>]>>]>>>>>>>>>]>>>]>]>>>]>>]F>>]>F]>>");
	buffer_queue.push("NA12885_S1.ch18.pile:chr18	15208082	c	76	,,..,.gT..G...,,.,,...,.....gg.g..gGG,g,gGGggG.G,gG,g,.gG,g.GG.GG,.,gg,g.gg.	B?3@HAH)=B@IBGJJGJG/JJHJGFGDDDEBFGDIJBDD=IID@JIJ?DJDDBGDJ8AHJHHHH@HD<DDDDDBB	]Z>]]]>]]]>>]]]]]]]]]]]>]]]]>>]>]]>>>]>]F>>>]>]>]>>]>]]>>>>]>>>>>]]]>>]>]>>]");
	buffer_queue.push("NA12886_S1.ch18.pile:chr18	15208082	c	89	.,.,,,,,,,..,.,G.,......,,,..,...,,,g.,.G.GG,,GggG..,g.gg,Gg,,,,gGG,,gGGGgg.,.,g.gg.gg.gg	5F(FDHHBHHFDIEE)GJI@EJEH?JBIGJGIHJGC?DFBCGJCBDIDBIGIBDI8DDID@@@0DIIBDBJE?DBH@B<8D?DFBD@?1	]]]>]X]]X>]>>]]>]]]]]]]]]]]]]]]>]]]]>>Z]>]>>]]>>>>]]Z>]>>]>>]]]>>>>]]>>>]>>]]]F>]>>]>>>>>");
	buffer_queue.push("NA12888_S1.ch18.pile:chr18	15208082	c	102	,...,..g,,.,,.....G..G,,,.,,.,..,..,.,...,,,,,..,,,G.,..g,...Gg.,GgggG.,gggGg,GGgg,,GGG,.ggggg,g,gggg	FD(@HCCHGI61IHF)H)>HG?FIIGIIAHJG@*IHGHAIBDD?;?ADIB?DIE><G?DJGDBBH<I8BDIJDBBDJBDJJD8D(GJGBHBDDBDBDD1D8@	]]]]]]]>>]]]]]>]>]>]]>]]]]]]]]>]]]]]]]]]]]]>]]]>>>]]>]]]]>]]]]>F]]>>>>>]]>>>>>]>>>>]]>>>]]>>>>>]>]>>>>");
	buffer_queue.push("NA12893_S1.ch18.pile:chr18	15208082	c	80	,.,.,,.,,,..G..,.,,,,,,..,.,,,,Gg,.g,.ggg.G,G..Gg,g.gGggGGG,.G.g.,gggg.g..gg,g,	@>CDHDFF:J)7DJGJGJJJJH;IACGFDBDCDDGBDEBDDD<BDGJJIB@BHDJDDJJHDGHBAHDA8DDD?FAABB<<	]]]]]]]]>]]]>]]]>]]]]]]]>]>]]]]>>]]F]]>>>>]>X>]]>>]>]>>>>>>>]]>]>]]>>>>]>]]>>]>]");
	for (int i = 0; i < params.sample_count; i++)
	{
		queue<string> temp_queue;
		temp_queue.push(buffer_queue.front());
		buffer_queue.pop();
		data_checkin(temp_queue, count_vector, MAX_NUM, i);
	}
	cout << "15208082 begin " << endl << endl;
	calculate_values_omp(params.end_condition, params.thread);
	cout << "15208082 finish " << endl << endl;*/
	/*buffer_queue.push("NA12877_S1.ch18.pile.gz:chr18	46593709	A	20  ...........GGGGGGGGG  HHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHH");
	buffer_queue.push("NA12878_S1.ch18.pile.gz:chr18	46593709	A	20  ...........GGGGGGGGG  HHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHH");
	buffer_queue.push("NA12879_S1.ch18.pile.gz:chr18	46593709	A	20  ...........GGGGGGGGG  HHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHH");
	buffer_queue.push("NA12880_S1.ch18.pile.gz:chr18	46593709	A 20  ...........GGGGGGGGG  HHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHH");
	buffer_queue.push("NA12882_2_S1.ch18.pile.gz:chr18	46593709	A	20  ...........GGGGGGGGG  HHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHH");
	buffer_queue.push("NA12883_S1.ch18.pile.gz:chr18	46593709	A	20  ...........GGGGGGGGG  HHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHH");
	buffer_queue.push("NA12885_S1.ch18.pile.gz:chr18	46593709	A	20  ...........GGGGGGGGG  HHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHH");
  buffer_queue.push("NA12886_S1.ch18.pile.gz:chr18	46593709	A	20  ...........GGGGGGGGG  HHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHH");
  buffer_queue.push("NA12888_S1.ch18.pile.gz:chr18	46593709	A	20  ...........GGGGGGGGG  HHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHH");
  buffer_queue.push("NA12893_S1.ch18.pile.gz:chr18	46593709	A	20  ...........GGGGGGGGG  HHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHH");

  for (int i = 0; i < params.sample_count; i++)
	{
		queue<string> temp_queue;
		temp_queue.push(buffer_queue.front());
		buffer_queue.pop();
		data_checkin(temp_queue, count_vector, MAX_NUM, i);
	}*/
	//cout << "46593709 begin " << endl << endl;
	//calculate_values_omp(params.end_condition, params.thread);
	//cout << "46593709 finish " << endl << endl;
	buffer_queue.push("chr22	16050036	a	7	C.CC.C.	II4HGEI	>8>>8>O");
	buffer_queue.push("chr22	16050036	a	7	.......	IEJJBF=	8O8O888");
  buffer_queue.push("chr22	16050036	a	3	C..	JFF	>88");
  buffer_queue.push("chr22	16050036	a	3	...	JIJ	]]8");
  buffer_queue.push("chr22	16050036	a	5	.....	IJIFC	888OO");
  buffer_queue.push("chr22	16050036	a	8	......^O.^8.	IIJCJ?CC	888888O8");
  buffer_queue.push("chr22	16050036	a	2	..	DG	8O");
  buffer_queue.push("chr22	16050036	a	9	CC.....CC	J<JJIJHHD	>>88O]8>>");
  buffer_queue.push("chr22	16050036	a	2	CC	J:	>>");
  buffer_queue.push("chr22	16050036	a	7	.....^8.^8.	JJHH@CC	]8O8888");
  for (int i = 0; i < params.sample_count; i++)
	{
		queue<string> temp_queue;
		temp_queue.push(buffer_queue.front());
		buffer_queue.pop();
		data_checkin(temp_queue, count_vector, MAX_NUM, i);
	}
	//cout << "55580488 begin " << endl << endl;
	calculate_values_omp(params.end_condition, params.thread);
	//cout << "55580488 finish " << endl << endl;
}
