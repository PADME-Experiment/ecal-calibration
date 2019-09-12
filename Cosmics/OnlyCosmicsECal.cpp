#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#include "TROOT.h"
#include "TFile.h"
#include "TChain.h"
#include "TTree.h"
#include "TBranch.h"
#include "TApplication.h"
#include "TMath.h"
#include "TH1I.h"
#include "TSpectrum.h"

#include "TRawEvent.hh"


const int nSamples = 1024;
const int lastGood = 1000;
const double pC2MeV = 15.;



struct Pulse{

  ULong64_t EvRunTime;
  TTimeStamp EvAbsTime;
  Double_t RMS;
  //UShort_t Samples[1024];
  //Double_t Smooth[1024];
  //Short_t Derivative[lastGood+1];
  UShort_t SamIndex[1024];
  std::vector<UShort_t> t0;
  std::vector<UShort_t> PeakPos;
  //std::vector<UShort_t> DerMinPos;
  std::vector<Double_t> Qpulse;
  std::vector<Double_t> QpulseW;
  Double_t Qtot;
  Double_t Baseline;
  Double_t MaxMin;
  Double_t BaseMin;
  //TH1I* Histo = new TH1I("histo","histo",1024,0,1024);
  Int_t Number;
  UInt_t Npeaks;
  UInt_t ZSupFlag;
  UInt_t IsBTF;
  UInt_t IsCR;
  UInt_t IsOffBeam;
  UShort_t t0Trig;
  UShort_t t50Trig;
  Double_t t50TrigD;

  Short_t t50TrigVec[4];
  Double_t t50TrigVecD[4];

  UShort_t PosX;
  UShort_t PosY;
  UChar_t Channel;
  UChar_t Board;
  Bool_t IsTrigger;
  Bool_t IsSignal;

};



UInt_t HexToBin_mask(UInt_t hexa,std::vector<UInt_t>* mask){

  UInt_t temp = hexa;
  UInt_t rest;
  //std::vector<UInt_t> mask;

  do{

    rest = temp%16;
    //printf("\t temp %08x %08x\n",temp,rest);    
    
    if(rest==0){
      mask->push_back(0);
      mask->push_back(0);
      mask->push_back(0);
      mask->push_back(0);
    }
    else if(rest==1){
      mask->push_back(1);
      mask->push_back(0);
      mask->push_back(0);
      mask->push_back(0);
    }
    else if(rest==2){
      mask->push_back(0);
      mask->push_back(1);
      mask->push_back(0);
      mask->push_back(0);
    }
    else if(rest==3){
      mask->push_back(1);
      mask->push_back(1);
      mask->push_back(0);
      mask->push_back(0);
    }
    else if(rest==4){
      mask->push_back(0);
      mask->push_back(0);
      mask->push_back(1);
      mask->push_back(0);
    }
    else if(rest==5){
      mask->push_back(1);
      mask->push_back(0);
      mask->push_back(1);
      mask->push_back(0);
    }
    else if(rest==6){
      mask->push_back(0);
      mask->push_back(1);
      mask->push_back(1);
      mask->push_back(0);
    }
    else if(rest==7){
      mask->push_back(1);
      mask->push_back(1);
      mask->push_back(1);
      mask->push_back(0);
    }
    else if(rest==8){
      mask->push_back(0);
      mask->push_back(0);
      mask->push_back(0);
      mask->push_back(1);
    }
    else if(rest==9){
      mask->push_back(1);
      mask->push_back(0);
      mask->push_back(0);
      mask->push_back(1);
    }
    else if(rest==10){
      mask->push_back(0);
      mask->push_back(1);
      mask->push_back(0);
      mask->push_back(1);
    }
    else if(rest==11){
      mask->push_back(1);
      mask->push_back(1);
      mask->push_back(0);
      mask->push_back(1);
    }
    else if(rest==12){
      mask->push_back(0);
      mask->push_back(0);
      mask->push_back(1);
      mask->push_back(1);
    }
    else if(rest==13){
      mask->push_back(1);
      mask->push_back(0);
      mask->push_back(1);
      mask->push_back(1);
    }
    else if(rest==14){
      mask->push_back(0);
      mask->push_back(1);
      mask->push_back(1);
      mask->push_back(1);
    }
    else if(rest==15){
      mask->push_back(1);
      mask->push_back(1);
      mask->push_back(1);
      mask->push_back(1);
    }

    
    temp/=16;
    
  }while((temp/16) > 0 || ((temp%16)!=0));
  
  Int_t size = mask->size();
  if(size<32){
    for(Int_t jj=size;jj<32;++jj){
      mask->push_back(0);
    }
  }
  
  //printf("size %d: ", int(mask->size()));
  //std::reverse(mask->begin(),mask->end());

  //for(int aa=0;aa<int(mask->size());++aa){printf("%d",mask[aa]);}
  //printf("\n");

  return mask->size();

}



int peak_search(Float_t sam_[1024],int NN,std::vector<UShort_t>* peakposSm_,
		std::vector<UShort_t>* t0sam_){

  int locminSm;

  for(int si=0;si<NN;++si){
    UInt_t sumSm = 0;
    for(int ij=0;ij<18;++ij){
	    if(sam_[si+ij] - sam_[si+ij+1] >= 0.){
	      sumSm++;
	    }
    }
    if(sumSm==18){
      //printf(" %d(%d) %f %f ",si,sumSm,(sam_[si]-sam_[si+locminSm])/2.,(sam_[si+2*locminSm]-sam_[si+locminSm]));
      locminSm = TMath::LocMin(30,&(sam_[si]));
      
      if(!((sam_[si]-sam_[si+locminSm])/2. < (sam_[si+2*locminSm]-sam_[si+locminSm]))){
	peakposSm_->push_back(si+locminSm);
	t0sam_->push_back(UShort_t(si));
	
	int aa = locminSm<30? (2*locminSm) : (locminSm+30);
	si += aa;
      }
    }
  } // end of pulse searching within smoothed window


  return int(peakposSm_->size());
}



int charge_evaluation(TADCChannel* chn_,Double_t base_,
		      std::vector<UShort_t> t0sam_,Double_t& qtot_,
		      std::vector<Double_t>& qpulse_,
		      std::vector<Double_t>& qpulsew_){

  // total charge
  for(int iq=int(t0sam_[0]);iq<lastGood;++iq){
    qtot_ += (base_-(chn_->GetSample(iq)))/4096./50*1E-9/1E-12; 
  } // end of total charge evaluation
  
  // single pulse charge
  for(int ip = 0;ip<int(t0sam_.size());++ip){
    Double_t qq = 0;
    Double_t qqw = 0;
    UShort_t last;
    last = ip<UShort_t(t0sam_.size())-1 ? t0sam_[ip+1] : lastGood;
    
    for(UShort_t iq = t0sam_[ip];iq<last;++iq){
      qq += (base_-(chn_->GetSample(iq)))/4096./50*1E-9/1E-12; 
      qqw += (base_-(chn_->GetSample(iq)))/4096./50*1E-9/1E-12; 
    }
    // subtration of other pulses with smaller t0
    for(int iip=0;iip<ip;++iip){
      qq -= qpulse_[iip]*(TMath::Exp(-1.*(t0sam_[ip]-t0sam_[iip])/300.)-TMath::Exp(-1.*(last-t0sam_[iip])/300.));
      qqw -= qpulse_[iip]*(TMath::Exp(-1.*(t0sam_[ip]-t0sam_[iip])/300.)-TMath::Exp(-1.*(last-t0sam_[iip])/300.));
    }
    //if(qq<0. || qqw<0.) {qq=0.; qqw=0.;}
    //printf("qq[%d/%d]=%f\n",ip,t0sam_.size(),qq);
    
    Double_t frac = 1.-(TMath::Exp(-1.*(last-t0sam_[ip])/300.));
    Double_t fracw = (1.-TMath::Exp(-1.*(last-t0sam_[ip])/300.))/(1.-TMath::Exp(-1.*(lastGood-t0sam_[ip])/300.));
    qq *= 1./frac;
    qqw *= 1./fracw;
    //if(t0sam_.size()==1)printf("t0 %d t1 %d, f %f (%f) fw %f (%f), Q %f\n",t0sam_[ip],last,frac,qq,fracw,qqw,qtot_);

    qpulse_.push_back(qq);
    qpulsew_.push_back(qqw);
    
  } // end of single pulse charge evaluation


  return 0;
}



int file_name(std::string lin, std::string& name);



int EventScreen(std::string name,std::string outname,
		 std::string mapname,int nevents){

  Pulse pulse;
  std::map< int, std::map<int,int> > B_Ch_XY;
  std::map< int, std::map<int,int> > B_Ch_SU;
  
  std::ifstream MapFile(mapname);
  std::string line;
  if(MapFile.is_open()){
    int SU,px,py,BN,CH;
    while(getline(MapFile,line)){
      std::stringstream(line) >> SU >> px >> py >> BN >> CH;
      B_Ch_XY[BN][CH] = 100*px+py;
      B_Ch_SU[BN][CH] = SU;
      //std::cout <<" || "<< SU <<" "<< B_Ch_XY[BN][CH]/100 <<" "<< B_Ch_XY[BN][CH]%100 <<" "<< BN <<" "<< CH<<std::endl;
    }
  }
  MapFile.close();
  
  gROOT->ProcessLine("#include <vector>");

  printf("new TFile\n");
  TFile *fileNTU = new TFile(Form("%s",outname.c_str()), "RECREATE");

  printf("new TTree\n");
  TTree *tree = new TTree("NTU","NTU");
  // event time from run start
  tree->Branch("EvRunTime",&(pulse.EvRunTime),"EvRunTime/l");
  // event unix time
  tree->Branch("EvAbsTime",&(pulse.EvAbsTime),"EvAbsTime");
  // charge of each single pulse in the window evaluated up to infinity
  tree->Branch("Qpulse",&(pulse.Qpulse));
  // charge of each single pulse in the window evaluated up to window end
  tree->Branch("QpulseW",&(pulse.QpulseW));

  // charge of each single pulse in the window evaluated up to infinity
  tree->Branch("t50TrigVec",pulse.t50TrigVec,"t50TrigVec/s");
  // charge of each single pulse in the window evaluated up to window end
  tree->Branch("t50TrigVecD",pulse.t50TrigVecD,"t50TrigVecD/D");

  // sample of pulses start within a window
  tree->Branch("t0",&(pulse.t0));
  // position of pulse minimum
  tree->Branch("PeakPos",&(pulse.PeakPos));
  // position of first sample that fullfil the condition on the pulse derivative
  // (a certain number of contiguous samples with a derivative larger than 
  // a value)
  //tree->Branch("DerMinPos",&(pulse.DerMinPos));
  // total charge in the window
  tree->Branch("Qtot",&(pulse.Qtot),"Qtot/D");
  // RMS of the event
  tree->Branch("RMS",&(pulse.RMS),"RMS/D");
  // position (double) of trigger 50% amplitude
  tree->Branch("t50TrigD",&(pulse.t50TrigD),"t50TrigD/D");
  // maximun - minimum in window, excluding last 24 samples
  tree->Branch("MaxMin",&(pulse.MaxMin),"MaxMin/D");
  // maximun - minimum in window, excluding last 24 samples
  tree->Branch("Baseline",&(pulse.Baseline),"Baseline/D");
  // maximun - minimum in window, excluding last 24 samples
  tree->Branch("BaseMin",&(pulse.BaseMin),"BaseMin/D");
  // TH1I with the samples
  //tree->Branch("Histo",&(pulse.Histo));//,"Histo/I");
  // event number
  tree->Branch("Number",&(pulse.Number),"Number/I");
  // number of peak in the acquired window
  tree->Branch("Npeaks",&(pulse.Npeaks),"Npeaks/i");
  // zero-suppression flag
  tree->Branch("ZSupFlag",&(pulse.ZSupFlag),"ZSupFlag/i");
  // BTF trigger flag
  tree->Branch("IsBTF",&(pulse.IsBTF),"IsBTF/i");
  // cosmic rays trigger flag
  tree->Branch("IsCR",&(pulse.IsCR),"IsCR/i");
  // delayed trigger flag
  tree->Branch("IsOffBeam",&(pulse.IsOffBeam),"IsOffBeam/i");
  // trigger signal t0 sample (start of signal rising)
  tree->Branch("t0Trig",&(pulse.t0Trig),"t0Trig/s");
  // sample of trigger 50% amplitude
  tree->Branch("t50Trig",&(pulse.t50Trig),"t50Trig/s");
  // crystal position x
  tree->Branch("PosX",&(pulse.PosX),"PosX/s");
  // crystal position y
  tree->Branch("PosY",&(pulse.PosY),"PosY/s");
  // channel number (holds for trigger and signal)
  tree->Branch("Channel",&(pulse.Channel),"Channel/b");
  // board number
  tree->Branch("Board",&(pulse.Board),"Board/b");
  // true for trigger samples
  tree->Branch("IsTrigger",&(pulse.IsTrigger),"IsTrigger/O");
  // true for signal samples
  tree->Branch("IsSignal",&(pulse.IsSignal),"IsSignal/O");



  /*
  printf("new TFile\n");
  TFile* fRawEv = new TFile(name.c_str());
  printf("Get\n");
  TTree* tRawEv = (TTree*)fRawEv->Get("RawEvents");
  printf("GetBranch\n");
  */
  TChain* tRawEv = new TChain("RawEvents");
  tRawEv->Add(name.c_str());
  TBranch* bRawEv = tRawEv->GetBranch("RawEvent");
  printf("TRawEvent\n");
  TRawEvent* rawEv = new TRawEvent();
  printf("SetAddress\n");
  bRawEv->SetAddress(&rawEv);

  UInt_t nevt = tRawEv->GetEntries();
  printf("TTree RawEvents contains %d events\n",nevt);

  // Set number of events to read
  UInt_t ntoread = nevt;
  if (nevents && nevents<(int)nevt) ntoread = nevents;

  printf("Reading the first %d events\n",ntoread);

  // filling index once and for all
  for(UShort_t si=0;si<1024;++si) pulse.SamIndex[si] = si;


  // some counters
  UInt_t nBTF = 0;
  UInt_t nCR = 0;
  UInt_t nOffBeam = 0;
  UInt_t nComplete = 0;
  UInt_t nIncomplete = 0;
  UInt_t nAuto = 0;
  UInt_t nNoAuto = 0;


  // loop on events //
  for(Int_t iev=0;iev<(Int_t)ntoread;iev++){

    //if(iev%10==0) printf("Reading event %d\n",iev);
    bRawEv->GetEntry(iev);

    //pulse.Number = iev;//

    UInt_t status = rawEv->GetEventStatus();
    ULong64_t RTime = rawEv->GetEventRunTime();
    //std::cout<<"RT "<<RTime<<std::endl;
    TTimeStamp ATime = rawEv->GetEventAbsTime();
    UInt_t TrKind = rawEv->GetEventTrigMask();

    std::vector<UInt_t> StatusMask;
    HexToBin_mask(status,&StatusMask);

    std::vector<UInt_t> TrigMask; 
    HexToBin_mask(TrKind,&TrigMask);

    if(TrigMask[0]) nBTF++;
    if(TrigMask[1]) nCR++;
    if(TrigMask[7]) nOffBeam++;

    if(StatusMask[0]) nComplete++;
    else nIncomplete++;

    if(StatusMask[1]) nAuto++;
    else nNoAuto++;



    if(!(TrigMask[1] && !TrigMask[0])) continue;



    UChar_t nBoards = rawEv->GetNADCBoards();
    // Loop over boards
    for(UChar_t b=0;b<nBoards;b++){
      //if(iev==118)std::cout<<"nBoards "<<b<<std::endl;
      // Show board info
      TADCBoard* adcB = rawEv->ADCBoard(b);
      //UChar_t bn = b;
      UChar_t bn = adcB->GetBoardId();

      //// ONLY CALORIMETER ////
      if((bn>9&&bn<14) || bn>23) continue;


      UChar_t nTrg = adcB->GetNADCTriggers();
      UChar_t nChn = adcB->GetNADCChannels();
      UInt_t Accepted = adcB->GetAcceptedChannelMask();

      std::vector<UInt_t> AccMask;
      HexToBin_mask(Accepted,&AccMask);

      if(iev==0) printf("Board %d, Ntrg %d, Nchn %d\n",bn,nTrg,nChn);
      //if(Accepted!=0) printf("iev %d; Accepted: %08x: \n",iev,Accepted);
      //for(int ii=int(AccMask.size()-1);ii>=0;--ii){printf("%u",AccMask[ii]);}
      //printf("\n");

      UShort_t t0trigch[32];
      UShort_t t50trigch[32];
      Double_t t50trigchd[32];


      UShort_t Samples[1024];
      Short_t Derivative[lastGood+1];

      // Loop over trigers
      for(UChar_t t=0;t<nTrg;t++){

	//UShort_t Samples[1024];

	TADCTrigger* trg = adcB->ADCTrigger(t);
	//UChar_t ch = t;
	UChar_t ch = trg->GetGroupNumber();
	//printf("| trg %d ",(int)ch);

	for(UShort_t tt=0;tt<trg->GetNSamples();tt++){
	  Samples[tt] = UShort_t(trg->GetSample(tt));
	  //(pulse.Histo)->Fill(trg->GetSample(tt));
	  Derivative[0]=0;
	  if(tt>0 && tt<lastGood){
	    Derivative[tt] = UShort_t(trg->GetSample(tt+1) - trg->GetSample(tt-2));
	  }
	}

	std::vector<UShort_t> peakpos;	
	//std::vector<UShort_t> derminpos;	
	Double_t rms = TMath::RMS(lastGood,&(Samples[0]));
	Double_t max = TMath::MaxElement(lastGood,&(Samples[0]));
	Double_t min = TMath::MinElement(1024,&(Samples[0]));
	Double_t base = TMath::Mean(50,&(Samples[0]));

	Double_t mid = (base-min)/2.;
	Short_t t0trig;
	Short_t t50trig;
	Short_t ydist;

	// 20 is the number of neded samples for the rise time
	// 2 is because Derivative is actually the double of the derivative
	Double_t Dy = (base-min)/50.;
	for(int tt=0;tt<trg->GetNSamples()-20;++tt){
	  if(Double_t(-1.*Derivative[tt]) > Dy
	     && Double_t(-1.*Derivative[tt+1]) > Dy
	     && Double_t(-1.*Derivative[tt+2]) > Dy
	     && Double_t(-1.*Derivative[tt+3]) > Dy
	     ){

	    //derminpos.push_back(tt);
	    t0trig = UShort_t(tt);
	    UShort_t minpos = TMath::LocMin(200,&(Samples[tt]));
	    peakpos.push_back(minpos+tt);
	    ydist = mid;

	    for(int tti=tt;tti<minpos+tt;++tti){
	      if(fabs(Samples[tti] - (base - mid)) < ydist){
		t50trig = UShort_t(tti);
		ydist = fabs(Samples[tti] - (base - mid));
	      } 
	    }
	    break;
	  }
	}


	UShort_t x0, x1, y0, y1;
	Double_t y50 = (base+min)/2.;       
	if((Samples[t50trig] - y50) > 0.){
	  x0 = t50trig; y0 = Samples[x0];
	  x1 = t50trig+1; y1 = Samples[x1];
	} else {
	  x0 = t50trig-1; y0 = Samples[x0];
	  x1 = t50trig; y1 = Samples[x1];
	}
	Double_t x50 = (double(y50)-double(y0))/(double(y1)-double(y0)) + double(x0);

	//printf("b %d ch %d, x0 %d x1 %d, x50 %f (%f)\n",bn,ch,x0,x1,x50,y50);

	for(int nn=0;nn<8;++nn){
	  t0trigch[8*ch+nn] = t0trig;
	  t50trigch[8*ch+nn] = t50trig;
	  t50trigchd[8*ch+nn] = x50;
	}


	pulse.RMS = rms;
	pulse.EvRunTime = RTime;
	pulse.EvAbsTime = ATime;
	pulse.MaxMin = max - min;
	pulse.Baseline = base;
	pulse.BaseMin = base - min;
	pulse.Number = iev;
	pulse.PeakPos = peakpos;
	//pulse.DerMinPos = derminpos;
	pulse.Npeaks = UInt_t(peakpos.size());
	pulse.Channel = ch;
	pulse.t0Trig = t0trig;
	pulse.t50Trig = t50trig;
	pulse.t50TrigD = x50;
	pulse.PosX = 0;
	pulse.PosY = 0;
	pulse.Board = bn;
	pulse.ZSupFlag = 1;
	pulse.IsBTF = TrigMask[0];
	pulse.IsCR = TrigMask[1];
	pulse.IsOffBeam = TrigMask[7];
	pulse.IsTrigger = true;
	pulse.IsSignal = false;

	tree->Fill();
      }//end of loop over triggers
      

      // Loop over channels
      for(UChar_t c=0;c<nChn;c++){

	//UShort Samples[1024];

	TADCChannel* chn = adcB->ADCChannel(c);
	UChar_t ch = chn->GetChannelNumber();

	// only non 0-supp channels are considered
	if(AccMask[ch]==0) continue;

	Float_t sam[1024];

       	for(UShort_t s=0;s<chn->GetNSamples();++s){
	  Samples[s] = UShort_t(chn->GetSample(s));
	  sam[s] = Float_t(chn->GetSample(s));
	  /*
	  Derivative[0] = 0;
	  if(s>0 && s<lastGood+1){
	    Derivative[s] = Short_t(chn->GetSample(s+1)-chn->GetSample(s-1));
	  }
	  */
	}
	TSpectrum *spec = new TSpectrum();
	spec->SmoothMarkov(sam,Int_t(chn->GetNSamples()),10);
	std::vector<UShort_t> peakposSm;
	std::vector<UShort_t> t0sam;
	//Double_t sambase = TMath::Mean(50,&(sam[0]));
	//Double_t sammin = TMath::MinElement(1024,&(sam[0]));
	//Double_t samDy = (sambase - sammin)/40.;

	// pulse searching within smoothed window
	int np = peak_search(sam,int(chn->GetNSamples()-600),&peakposSm,&t0sam);	

	
	std::vector<UShort_t> peakpos;	
	//std::vector<UShort_t> derminpos;	
	Double_t rms = TMath::RMS(lastGood,&(Samples[0]));
	Double_t rms45 = TMath::RMS(45,&(Samples[0]));
	Double_t max = TMath::MaxElement(lastGood,&(Samples[0]));
	Double_t min = TMath::MinElement(1024,&(Samples[0]));
	Double_t base = TMath::Mean(50,&(Samples[0]));
	Double_t qtot = 0;
	std::vector<Double_t> qpulse;
	std::vector<Double_t> qpulsew;

	// charges evaluation
	if(np>0){
	  charge_evaluation(chn,base,t0sam,qtot,qpulse,qpulsew);

	  //printf("qtot %f",qtot);
	  //for(int qq=0;qq<np;++qq){printf(" %f",qpulse[qq]);}
	  //printf("\n");
	}

	// remove all the pulses with a negative charge
	for(auto ic=qpulse.begin();ic!=qpulse.end();){
	  if(*ic<0.){
	    int ind = distance(qpulse.begin(),ic);

	    ic = qpulse.erase(ic);
	    t0sam.erase(t0sam.begin()+ind);
	    qpulsew.erase(qpulsew.begin()+ind);
	    peakposSm.erase(peakposSm.begin()+ind);

	  } else {
	    ++ic;
	  }
	}

	/*
	*/

	//for(int ijk=0;ijk<1024;++ijk){Smooth[ijk] = sam[ijk];}
	pulse.Qpulse = qpulse;
	pulse.QpulseW = qpulsew;
	pulse.Qtot = qtot;
	pulse.RMS = rms;
	pulse.EvRunTime = RTime;
	pulse.EvAbsTime = ATime;
	pulse.MaxMin = max - min;
	pulse.Baseline = base;
	pulse.BaseMin = base - min;
	pulse.Number = iev;
	pulse.t0 = t0sam;
	pulse.PeakPos = peakposSm;
	//pulse.DerMinPos = derminpos;
	pulse.Npeaks = UInt_t(qpulse.size());

	pulse.t0Trig = t0trigch[ch];
	pulse.t50Trig = t50trigch[ch];
	pulse.t50TrigD = t50trigchd[ch];

	pulse.PosX = B_Ch_XY[bn][ch]/100;
	pulse.PosY = B_Ch_XY[bn][ch]%100;
	pulse.Channel = ch;
	pulse.Board = bn;
	pulse.ZSupFlag = AccMask[ch];
	pulse.IsBTF = TrigMask[0];
	pulse.IsCR = TrigMask[1];
	pulse.IsOffBeam = TrigMask[7];
	pulse.IsTrigger = false;
	pulse.IsSignal = true;

	tree->Fill();
      }//end of loop over channels
      
    }
    // Clear event
    rawEv->Clear("C");
  }
  // end of loop on events //

  fileNTU->cd();
  //tree->Print();
  tree->Write();
  fileNTU->Close();

  delete rawEv;
  delete bRawEv;
  delete tRawEv;
  //fRawEv->Close();
  //delete fRawEv;

  printf("Statistics:\n\tBTF trigger %d\n\tCR trigger %d\n\tOff beam trigger %d\n\tStatus (in)complete (%d)%d\n\t(no)autopass (%d)%d\n",nBTF,nCR,nOffBeam,nIncomplete,nComplete,nNoAuto,nAuto);

  return 0;

}



int main(int argc, char* argv[])
{
  int nevents=0;
  int c;
  TString inputFileName;
  TObjArray inputFileNameList;
  struct stat filestat;

  std::string infile = "rawdata.root";
  std::string outfile = "out";
  std::string mapfile = "ECal_map_2.txt";
  int verbose = 0;
  bool iOpt = false;
  bool oOpt = false;

  // Parse options
  while ((c = getopt (argc, argv, "i:l:o:n:v:h")) != -1) {
    switch (c)
      {
      case 'i':
	inputFileNameList.Add(new TObjString(optarg));
        infile = optarg;
        fprintf(stdout,"Set input data file to '%s'\n",infile.c_str());
	iOpt = true;
        break;
      case 'l':
	if ( stat(Form(optarg),&filestat) == 0 ) {
	  fprintf(stdout,"Reading list of input files from '%s'\n",optarg);
	  infile = optarg;
	  std::ifstream inputList(optarg);
	  while( inputFileName.ReadLine(inputList) ){
	    //if ( stat(Form(inputFileName.Data()),&filestat) == 0 ) {
	    inputFileNameList.Add(new TObjString(inputFileName.Data()));
	    fprintf(stdout,"Added input data file '%s'\n",inputFileName.Data());
	      //} 
	    /* else if(InputFileName.CompareTo("")){
	       fprintf(stdout,"WARNING: file '%s' is not accessible\n",inputFileName.Data());
	       }*/
	  }
	} else {
	  fprintf(stdout,"WARNING: file list '%s' is not accessible\n",optarg);
	}
        break;
      case 'o':
        outfile = optarg;
        fprintf(stdout,"Set output data file name to '%s'.root\n",outfile.c_str());
	oOpt = true;
        break;
      case 'n':
        if ( sscanf(optarg,"%d",&nevents) != 1 ) {
          fprintf (stderr, "Error while processing option '-n'. Wrong parameter '%s'.\n", optarg);
          exit(1);
        }
        if (nevents<0) {
          fprintf (stderr, "Error while processing option '-n'. Required %d events (must be >=0).\n", nevents);
          exit(1);
        }
	if (nevents) {
	  fprintf(stdout,"Will read first %d events in file\n",nevents);
	} else {
	  fprintf(stdout,"Will read all events in file\n");
	}
        break;
      case 'v':
        if ( sscanf(optarg,"%d",&verbose) != 1 ) {
          fprintf (stderr, "Error while processing option '-v'. Wrong parameter '%s'.\n", optarg);
          exit(1);
        }
        if (verbose<0) {
          fprintf (stderr, "Error while processing option '-v'. Verbose level set to %d (must be >=0).\n", verbose);
          exit(1);
        }
        fprintf(stdout,"Set verbose level to %d\n",verbose);
        break;
      case 'h':
        fprintf(stdout,"\nReadSource [-i input root file] [-v verbosity] [-h]\n\n");
        fprintf(stdout,"  -i: define an input file in root format\n");
        fprintf(stdout,"  -l: define an input filelist\n");
	fprintf(stdout,"  -o: define an output file in root format\n");
        fprintf(stdout,"  -n: define number of events to read from input file (0: all events)\n");
        fprintf(stdout,"  -v: define verbose level\n");
        fprintf(stdout,"  -h: show this help message and exit\n\n");
        exit(0);
      case '?':
        if (optopt == 'v') {
          // verbose with no argument: just enable at minimal level
          verbose = 1;
          break;
        } else if (optopt == 'i' || optopt == 'l' || optopt == 'o')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint(optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,"Unknown option character `\\x%x'.\n",optopt);
        exit(1);
      default:
        abort();
      }
  }

  printf("FillHisto\n");

  //EventScreen(infile.c_str(),outfile.c_str(),mapfile.c_str(),nevents);

  Int_t nFile = inputFileNameList.GetEntries();
 
  

  if(iOpt){ // -i option
    for (Int_t iFile = 0; iFile < nFile; iFile++) {
      
      fprintf(stdout,"\n\n%4d - %s\n",iFile,((TObjString*)inputFileNameList.At(iFile))->GetString().Data());
      char* fname = Form("%s",((TObjString*)inputFileNameList.At(iFile))->GetString().Data());
      
      
      if(!oOpt){
	file_name(fname,outfile);
	outfile = Form("NTU_%s",outfile.c_str());
      }
      outfile = Form("../../cosmics/output/%s.root",outfile.c_str());
      std::cout<<"Output file name "<<outfile<<std::endl;
      
      EventScreen(((TObjString*)inputFileNameList.At(iFile))->GetString().Data(),outfile.c_str(),mapfile.c_str(),nevents);
    }  

  } else { // -l option

    std::string outlistname;
    if(!oOpt){
      file_name(infile,outlistname);
      outlistname = Form("../../cosmics/output/CR_%s.list",outlistname.c_str());
    } else {
      outlistname = Form("../../cosmics/output/CR_%s.list",outfile.c_str());
    }
    std::cout<<"OPENING  "<<outlistname<<std::endl;
    std::ofstream outlistfile;
    outlistfile.open(outlistname);

    for (Int_t iFile = 0; iFile < nFile; iFile++) {
      std::string Ofile;

      fprintf(stdout,"\n\n================================\n%4d - %s\n",iFile,((TObjString*)inputFileNameList.At(iFile))->GetString().Data());
      char* fname = Form("%s",((TObjString*)inputFileNameList.At(iFile))->GetString().Data());

      file_name(fname,Ofile);
      outlistfile<<"NTU_"<<Ofile<<".root"<<std::endl;
      Ofile = Form("../../cosmics/output/NTU_%s.root",Ofile.c_str());

      EventScreen(((TObjString*)inputFileNameList.At(iFile))->GetString().Data(),Ofile,mapfile.c_str(),nevents);
    }

    outlistfile.close();

  }
  
  
  exit(0);

}



int file_name(std::string lin, std::string& name){

  std::size_t begin, end;
  
  begin = lin.find_last_of("/\\");
  //std::cout<<"begin "<<begin<<std::endl;
  //if(begin == std::string::npos) begin = 0;

  end = lin.find_last_of(".");
  name = lin.substr(begin+1, end - begin - 1);

  return 0;
}
