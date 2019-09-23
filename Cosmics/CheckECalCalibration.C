/*
This macro is meant to works w/ CR for all the ECal channels present in the given file(s). Obviously the macro will work for each kind of trigger, but it will give meaningful results only for the CRs.

This macro draws:
- the MPV histo of the landau fits to the CR spectra; all events, with no requirements on the verticality of the CR
- the MPV 2D map of the landau fits to the CR spectra; all events, with no requirements on the verticality of the CR
- the MPV histo of the landau fits to the CR spectra, only when the CR passes vertically through the crystal (condition given by the passage of the CR through the SU above and the SU below); there are 2 histos one that considers also the SU on the ECal edges (in this cases the two crystal above/below are used)
- the MPV 2D map of the landau fits to the CR spectra, only when the CR passes vertically through the crystal (condition given by the passage of the CR through the SU above and the SU below)
- the efficiency histo of all the channels (it is evaluated only on the crystals not on the edges)
- the efficiency 2D map of all the channels (it is evaluated only on the crystals not on the edges)
- the distribution of the ratio between the MPV values of this specific file(s) wrt MPV obtained from run_0000000_20190111_105312
- the spectra for all the ECal channels present in the input file(s), if verbose = true

The ma cro writes in an output file (outfile, check where it is written) the values of the MPV for vertical CRs (needed to improve the equalisation), lines are of the kind:
posX posY Board ChInBoard MPV

Variables are:
- data_file = path/name of file to open; the files must be the ones produced by the analysis, not the merged ones
- cuts = cuts to apply to the data
- verbose = if true it draws all the CR spectra for the selected channels
- save = if true it saves the cumulative canvases (MPV distr with gaussian fit, vertical MPV distr w/ gaussian fit, MPV map, vertical MPV map, efficiency distr w/ fit, efficiency map); figures are saved in figures/; formats are pdf, eps and root (I don't know why, but the root format is different from the plot produced by the macro, but if you resave them by hands they are ok)



Run as:
- using a single root file (in verbose mode and saving figures)
root [0] .L CheckECalCalibration.C+
root [1] CheckECalCalibration("/home/daq/Analysis/EventScreen_run_0000000_20181119_154836_lvl1_03_026.root","IsCR",true,true)

- using a list file (not in verbose mode and w/o saving figures)
root [0] .L CheckECalCalibration.C+
root [1] CheckECalCalibration("/home/daq/Analysis/cosmics/CR_CRrun.list","IsCR",false,false)
*/

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sys/io.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <set>

#include "TH1D.h"
#include "TH2D.h"
#include "TF1.h"
#include "TFile.h"
#include "TCanvas.h"
#include "TChain.h"
#include "TCut.h"
#include "TAxis.h"
#include "TPaveStats.h"
#include "TPaveText.h"
#include "TStyle.h"
#include "TROOT.h"
#include "TLine.h"
#include "TMath.h"
#include "TLegend.h"



void DrawCrystals(TCanvas *Canv);

int DoCalculations(char const* dataFile, TCut Cut, char const* idvar,std::map<int, TH1D* >& XYQspec,std::map<int, TH1D* >& XYQspecvert,std::map<int, int >& XY_pass,std::map<int, int >& XY_seen);

string GetFileName(string path);

int PosInfo(int x_y);

bool IsPathExisting(const std::string &s);



void CheckECalCalibration(char const* data_file, TCut cuts, bool verbose=false, bool save=false){

  // select a variable to idenfy the events
  char const *idvariable = "Number";
  //char const *idvariable = "EvRunTime";
  TCut trig_type = "IsCR";

  gStyle->SetOptFit(10111);
  //gStyle->SetOptFit();
  gStyle->SetOptStat(0);
  //gStyle->SetOptStat(1111111);

  // map with all the charges
  std::map<int, TH1D* > xy_Qspec;
  // map with charges only of vertical CR
  std::map<int, TH1D* > xy_Qspecvert;
  // map 
  std::map<int, int> xy_passing;
  // map 
  std::map<int, int> xy_seen;


  std::ofstream outfile;
  string fname = GetFileName(data_file);
  if(!IsPathExisting("../../cosmics/calibfiles")){
    std::cout<<"\n\nCreating the directory ../../cosmics/calibfiles/\n";
    if(mkdir("../../cosmics/calibfiles",S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) cout<<"\nImpossible to create the directory ../../cosmics/calibfiles/.\n";
  }
  string ofname = "../../cosmics/calibfiles/equalisation_constants_" + fname + ".dat";
  outfile.open(ofname);

  TString filename = TString(data_file);
  if(filename.Contains("list")){ // input file is a list file
    std::cout<<"\n\nList file in input\n"<<std::endl;

    std::ifstream DataFile(data_file);
    std::string line;
    if(DataFile.is_open()){
      while(getline(DataFile,line)){
	std::cout<<"\n*****************************************\n"
		 <<"==> Opening file: "<<line<<std::endl<<std::endl;

	int r = DoCalculations(line.c_str(),trig_type+cuts,idvariable,xy_Qspec,xy_Qspecvert,xy_passing,xy_seen);
      }
    }
    DataFile.close();

  } else { // input file is a root file
    DoCalculations(data_file,trig_type+cuts,idvariable,xy_Qspec,xy_Qspecvert,xy_passing,xy_seen);
  }


  TH1D* MPVhist = new TH1D("MPV distr","",700,0,700);  
  TH1D* MPVverthist = new TH1D("vertical MPV distr","",700,0,700);
  TH1D* MPVcentrverthist = new TH1D("inner vertical MPV distr","inner verical MPV distr",700,0,700);
  TH1D* Effhist = new TH1D("Efficiency distr","",2200,0,110);  
  TH1D* MPVratio = new TH1D("MPV ratio","",1000,-5.,5.);  
  TH2D* ECal = new TH2D("MPV map",""/*"MPV map"*/,29,0,29,29,0,29);
  TH2D* ECalVert = new TH2D("vertical MPV map",""/*"vertical MPV map"*/,29,0,29,29,0,29);
  TH2D* ECalEff = new TH2D("Efficiency map",""/*"Efficiency map"*/,29,0,29,29,0,29);
  TH2D* ECalRatio = new TH2D("MPV ratio map","",29,0,29,29,0,29);


  // link between position and board/channel
  std::map<int, int> xy_BCh;
  std::ifstream mapfile("ECal_map_2.txt");
  std::string line;
  if(mapfile.is_open()){
    int SU,px,py,BN,CH;
    while(getline(mapfile,line)){
      std::stringstream(line) >> SU >> px >> py >> BN >> CH;
      xy_BCh[px*100+py] = 100*BN+CH;
      std::cout<< SU <<", x "<< px <<" y "<< py <<" ("<< px*100+py<<"), B "<< xy_BCh[px*100+py]/100 <<", Ch "<<xy_BCh[px*100+py]%100<<std::endl;
    }
  }
  mapfile.close();


  // link between position and MPV from file to check that everything is done consistently:
  // MPV from fit is divided from the value written in the file (1 is expected as result)
  std::map<int, double> xy_MPV;
  std::ifstream constfile("../../padme-fw/PadmeReco/config/Calibration/ECalCalibConst.txt");
  if(constfile.is_open()){
    int Px,Py,BN,CH;
    double mpv;
    while(getline(constfile,line)){
      std::stringstream(line) >> Px >> Py >> BN >> CH >> mpv;
      xy_MPV[Px*100+Py] = mpv;
      std::cout<<"x "<< Px <<" y "<< Py <<" MPV "<<mpv<<std::endl;
    }
  } else {
    std::cout<<"file with consants is not existing!!!"<<std::endl<<std::endl; 
  }
  constfile.close();




  std::map<int, TH1D* >::iterator xyQ_it2;
  for(xyQ_it2=xy_Qspec.begin();xyQ_it2!=xy_Qspec.end();++xyQ_it2){

    if(verbose){
      const char *htit = xyQ_it2->second->GetTitle();
      std::cout<<"titolo "<<htit<<" || "<<xyQ_it2->first<<std::endl;
      TCanvas* hcanv = new TCanvas(htit,htit,800,600);
      hcanv->cd();
      xyQ_it2->second->GetXaxis()->SetTitle("Charge [pC]");
      xyQ_it2->second->Draw();
      xyQ_it2->second->SetLineColor(kBlue);
      xyQ_it2->second->SetLineWidth(2);


      if(xy_Qspecvert[xyQ_it2->first]){
	xy_Qspecvert[xyQ_it2->first]->Draw("same");
	xy_Qspecvert[xyQ_it2->first]->SetLineColor(kRed);
	xy_Qspecvert[xyQ_it2->first]->SetLineWidth(2);
	xy_Qspecvert[xyQ_it2->first]->SetLineStyle(kDashed);
      }
    }

    xyQ_it2->second->GetXaxis()->SetRangeUser(30.,900.);
    Double_t maxpos = xyQ_it2->second->GetBinCenter(xyQ_it2->second->GetMaximumBin());
    xyQ_it2->second->GetXaxis()->SetRangeUser(-100.,900.);

    if(!verbose){
      xyQ_it2->second->Fit("landau","0","",maxpos-100.,900.);
      if(xy_Qspecvert[xyQ_it2->first]){
	xy_Qspecvert[xyQ_it2->first]->Fit("landau","0","",maxpos-100.,900.);
      }
    } else {
      xyQ_it2->second->Fit("landau","","",maxpos-100.,900.);
      if(xy_Qspecvert[xyQ_it2->first]){
	xy_Qspecvert[xyQ_it2->first]->Fit("landau","","",maxpos-100.,900.);
      }
    }

    // all CR Landau fit and histo filling
    TF1* fit = xyQ_it2->second->GetFunction("landau");

    if(xyQ_it2->second->GetEntries()>2){
      MPVhist->Fill(fit->GetParameter(1));
      ECal->Fill((xyQ_it2->first)/100,(xyQ_it2->first)%100,fit->GetParameter(1));
      //MPVhist->Fill(maxpos);
      //ECal->Fill((xyQ_it2->first)/100,(xyQ_it2->first)%100,maxpos);
    }

    // vertical CR Landau fit, histo filling and writing file w/ constants
    if(xy_Qspecvert[xyQ_it2->first]
       && (xy_Qspecvert[xyQ_it2->first])->GetEntries()>2){

      TF1* fit2 = xy_Qspecvert[xyQ_it2->first]->GetFunction("landau");
      
      MPVverthist->Fill(fit2->GetParameter(1));
      if(PosInfo(xyQ_it2->first)==0) MPVcentrverthist->Fill(fit2->GetParameter(1));
      if(fit2->GetParameter(1)>0.){
	ECalVert->Fill((xyQ_it2->first)/100,(xyQ_it2->first)%100,fit2->GetParameter(1));
      }
      //ECalVert->Fill((xyQ_it2->first)/100,(xyQ_it2->first)%100,(xy_Qspecvert[xyQ_it2->first])->GetEntries());

      // Check of MPV values
      double mpv_rat = (fit2->GetParameter(1))/xy_MPV[xyQ_it2->first];
      std::cout<<"ratio "<<mpv_rat;
      if((mpv_rat < 0.75) || (mpv_rat > 1.25)){
	std::cout<<" <- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -";
      } else {
	ECalRatio->Fill((xyQ_it2->first)/100,(xyQ_it2->first)%100,100.*mpv_rat);
      }
      std::cout<<std::endl;
      MPVratio->Fill(mpv_rat);
      // end of MPV check


      // write MPV values (equalisation constants) in output file
      outfile<<(xyQ_it2->first)/100<<" "<<(xyQ_it2->first)%100<<" " // position x (col), position y (row)
	     <<((xy_BCh[xyQ_it2->first])/100)<<" " // Board ID
	     <<((xy_BCh[xyQ_it2->first])%100)<<" " // Channel ID (0->32 in board)
	     <<fit2->GetParameter(1)<<std::endl; // Vertical CR spectrum MPV

    }


    std::cout<<"pos "<<(xyQ_it2->first)<<" (x: "<<(xyQ_it2->first)/100
	     <<", y: "<<(xyQ_it2->first)%100<<", "
	     <<xyQ_it2->second->GetEntries()<<" entries";
    if(xy_Qspecvert[xyQ_it2->first]){
      std::cout<<", "<<xy_Qspecvert[xyQ_it2->first]->GetEntries()
	       <<" vert. entries";
      if(xy_Qspecvert[xyQ_it2->first]->GetEntries() < 100){
	std::cout<<" !!!FEW EVNTS!!! <--------------------";
      }
    }
    std::cout<<"): maxpos "<<maxpos<<" pC"
	     <<std::endl<<std::endl;

  }
  
  // closing the file with the equalisation constants
  outfile.close();


  // filling efficiency plots
  std::map<int, int >::iterator xypass_it2;
  for(xypass_it2=xy_passing.begin();xypass_it2!=xy_passing.end();++xypass_it2){
    double eff = 100.*double(xy_seen[xypass_it2->first])/double(xypass_it2->second);
    Effhist->Fill(eff);
    ECalEff->Fill(xypass_it2->first/100,xypass_it2->first%100,eff);
    std::cout<<"x: "<<xypass_it2->first/100<<", y "<<xypass_it2->first%100
	     <<" (using "<<xypass_it2->second<<" events): eff "<<eff<<std::endl;
  }


  // histogram of the MPV of vertical CR charge spectra
  TCanvas* vertMPVCanv = new TCanvas("vert MPV histo","vert MPV histo",800,600);

  MPVverthist->GetXaxis()->SetTitle("Charge [pC]");
  MPVverthist->Rebin(4);
  double bwVert = MPVverthist->GetBinWidth(0);
  MPVverthist->GetYaxis()->SetTitle(Form("Counts/(%1.0f pC)",bwVert));
  TF1* fgaus0 = new TF1("fgaus0","[0]*exp(-0.5*((x-[1])/[2])**2)",-100.,900.);
  //TF1* fgaus0 = new TF1("fgaus0","gaus");
  fgaus0->SetParName(0,"A_{all}");
  fgaus0->SetParameter(0,50.);
  fgaus0->SetParLimits(0,3.,600.);
  fgaus0->SetParName(1,"#mu_{all}");
  fgaus0->SetParameter(1,300.);
  fgaus0->SetParLimits(1,100.,600.);
  fgaus0->SetParName(2,"#sigma_{all}");
  fgaus0->SetParameter(2,30.);
  fgaus0->SetParLimits(2,1.,100.);

  //TF1* fgaus1 = new TF1("fgaus1","gaus");
  TF1* fgaus1 = new TF1("fgaus1","[0]*exp(-0.5*((x-[1])/[2])**2)",-100.,900.);
  fgaus1->SetParName(0,"A_{inner}");
  fgaus1->SetParameter(0,50.);
  fgaus1->SetParLimits(0,3.,600.);
  fgaus1->SetParName(1,"#mu_{inner}");
  fgaus1->SetParameter(1,300.);
  fgaus1->SetParLimits(1,100.,600.);
  fgaus1->SetParName(2,"#sigma_{inner}");
  fgaus1->SetParameter(2,30.);
  fgaus1->SetParLimits(2,1.,100.);

  

  std::cout<<"\n\n == SUs VERTICAL CR MPV FIT ==\n"<<std::endl;
  MPVverthist->Fit("fgaus0","0","",-100.,900.);
  MPVverthist->Draw();
  MPVverthist->SetLineColor(kBlue);
  MPVverthist->SetLineWidth(2);
  MPVverthist->SetFillStyle(3013);
  MPVverthist->SetFillColor(MPVverthist->GetLineColor());
  fgaus0->Draw("sames");
  fgaus0->SetLineColor(MPVverthist->GetLineColor());
  fgaus0->SetLineStyle(kDashed);
  
  gPad->Update();
  TPaveStats* st0 = (TPaveStats*) MPVverthist->GetListOfFunctions()->FindObject("stats");
  st0->SetFillStyle(0);
  st0->SetX1NDC(0.6);
  st0->SetX2NDC(0.9);
  st0->SetY1NDC(0.7);
  st0->SetY2NDC(0.9);
  st0->SetTextColor(MPVverthist->GetLineColor());
  gPad->Update();
  gPad->Modified();

  vertMPVCanv->cd();

  // vertical inner CR //
  double bwVertC = MPVcentrverthist->GetBinWidth(0);
  MPVcentrverthist->Rebin(4);
  std::cout<<"\n\n == INNER SUs VERTICAL CR MPV FIT ==\n"<<std::endl;
  MPVcentrverthist->Fit("fgaus1","0","",-100.,900.);
  //MPVcentrverthist->SetFillStyle(4000);
  MPVcentrverthist->Draw("][sames");
  MPVcentrverthist->SetLineColor(kRed);
  MPVcentrverthist->SetLineWidth(2);
  MPVcentrverthist->SetFillStyle(3013);
  MPVcentrverthist->SetFillColor(MPVcentrverthist->GetLineColor());
  fgaus1->Draw("SAMES");
  fgaus1->SetLineColor(MPVcentrverthist->GetLineColor());
  fgaus1->SetLineStyle(kDashed);

  gPad->Update();
  TPaveStats* st1 = (TPaveStats*) MPVcentrverthist->FindObject("stats");
  st1->SetFillStyle(0);
  st1->SetX1NDC(0.6);
  st1->SetX2NDC(0.9);
  st1->SetY1NDC(0.5);
  st1->SetY2NDC(0.7);
  st1->SetTextColor(MPVcentrverthist->GetLineColor());
  gPad->Update();
  gPad->Modified();


  // histogram of the MPV of the CR charge spectra
  TCanvas* MPVCanv = new TCanvas("MPV histo","MPV histo",800,600);    
  MPVCanv->cd();
  MPVhist->GetXaxis()->SetTitle("Charge [pC]");
  MPVhist->Rebin(8);
  std::cout<<"\n\n == SUs CR MPV FIT ==\n"<<std::endl;
  MPVhist->Fit("gaus");
  MPVhist->Draw();


  // histogram of the MPV ratios (current_value/file_value)
  TCanvas* MPVRatioCanv = new TCanvas("MPV ratio canv","MPV ratio canv",800,600);    
  MPVRatioCanv->cd();
  MPVratio->GetXaxis()->SetTitle("Ratio");
  MPVratio->Draw();
  MPVratio->SetLineColor(kBlue);
  MPVratio->SetLineWidth(2);
  MPVratio->SetFillStyle(3013);
  MPVratio->SetFillColor(MPVratio->GetLineColor());



  // histogram of the efficiencies from vertical CR //
  TCanvas* effCanv = new TCanvas("efficiency","efficiency",800,600);
  effCanv->cd()->SetLogy();
  Effhist->GetXaxis()->SetTitle("Efficiency [%]");
  Effhist->GetYaxis()->SetTitle("Counts/(0.1 %)");
  //Effhist->Rebin(4);
  //TF1* flandau = new TF1("flandau","[0]*TMath::Landau(-x,[1],[2])",0,110);
  TF1* flandau = new TF1("flandau","[0]*::ROOT::Math::landau_pdf((-x+[1])/[2])",0,110);
  //Effhist->Fit("landau");
  flandau->SetParameters(500.,600.,1.);
  flandau->SetParName(0,"A");
  flandau->SetParLimits(0,50.,10000.);
  flandau->SetParName(1,"MPV");
  flandau->SetParLimits(1,98.,103.);
  flandau->SetParName(2,"#sigma");
  flandau->SetParLimits(2,0.01,5.);
  std::cout<<"\n\n == EFFICIENCY DISTRIBUTION FIT ==\n"<<std::endl;
  Effhist->Fit("flandau","0");
  Effhist->Draw();
  Effhist->SetLineColor(kBlue);
  Effhist->SetLineWidth(2);
  Effhist->SetFillStyle(3013);
  Effhist->SetFillColor(Effhist->GetLineColor());
  flandau->Draw("same");
  flandau->SetLineColor(Effhist->GetLineColor());

  gPad->Update();
  TPaveStats* st2 = (TPaveStats*) Effhist->FindObject("stats");
  st2->SetFillStyle(0);
  st2->SetX1NDC(0.25);
  st2->SetX2NDC(0.55);
  st2->SetY1NDC(0.7);
  st2->SetY2NDC(0.9);
  //st2->SetTextColor(MPVverthist->GetLineColor());
  gPad->Update();
  gPad->Modified();
  
  TH1D* Effhist2 = (TH1D*) Effhist->Clone();
  TPad *pad = new TPad("pad","pad",0.15,0.22,0.65,0.67);
  pad->Draw();
  pad->cd();
  pad->SetBottomMargin(0.15);
  pad->SetLeftMargin(0.13);
  pad->SetRightMargin(0.03);
  Effhist2->Draw();
  Effhist2->GetXaxis()->SetRangeUser(97.,101.);
  Effhist2->SetStats(0);
  Effhist2->GetXaxis()->SetLabelSize(0.06);
  Effhist2->GetXaxis()->SetTitleSize(0.07);
  Effhist2->GetXaxis()->SetTitleOffset(0.8);
  Effhist2->GetYaxis()->SetLabelSize(0.06);
  Effhist2->GetYaxis()->SetTitleSize(0.07);
  Effhist2->GetYaxis()->SetTitleOffset(0.75);
  Effhist2->SetTitle(" ");
  flandau->Draw("same");
  flandau->SetLineStyle(kDashed);
  

  // to have only 4 digits inside the SU square in the ECal map
  // for the event charge
  gStyle->SetPaintTextFormat("4.0f");

  // ECal map with MPV from CR
  TCanvas* ECalCanv = new TCanvas("MPV canv","MPV canv",800,800);
  ECalCanv->cd();
  ECalCanv->SetLeftMargin(0.05);
  ECalCanv->SetRightMargin(0.15);
  ECalCanv->SetBottomMargin(0.05);
  ECalCanv->SetTopMargin(0.08);
  ECal->Draw("colz");
  ECal->Draw("text same");
  ECal->SetMarkerSize(0.9);
  ECal->GetXaxis()->SetNdivisions(29);
  ECal->GetYaxis()->SetNdivisions(29);
  ECal->GetXaxis()->SetTickLength(0);
  ECal->GetYaxis()->SetTickLength(0);
  ECal->GetXaxis()->CenterLabels();
  ECal->GetYaxis()->CenterLabels();
  ECal->GetXaxis()->SetLabelSize(0.02);
  ECal->GetYaxis()->SetLabelSize(0.02);
  ECal->GetZaxis()->SetTitle("Charge [pC]");
  ECal->GetZaxis()->SetTitleOffset(1.2);

  DrawCrystals(ECalCanv);


  // ECal map with MPV from vertical CR
  TCanvas* ECalCanv2 = new TCanvas("vert MPV canv","vert MPV canv",800,800);
  ECalCanv2->cd();
  ECalCanv2->SetLeftMargin(0.05);
  ECalCanv2->SetRightMargin(0.15);
  ECalCanv2->SetBottomMargin(0.05);
  ECalCanv2->SetTopMargin(0.08);
  ECalVert->Draw("colz");
  ECalVert->Draw("text same");
  ECalVert->SetMarkerSize(0.9);
  ECalVert->GetXaxis()->SetNdivisions(29);
  ECalVert->GetYaxis()->SetNdivisions(29);
  ECalVert->GetXaxis()->SetTickLength(0);
  ECalVert->GetYaxis()->SetTickLength(0);
  ECalVert->GetXaxis()->CenterLabels();
  ECalVert->GetYaxis()->CenterLabels();
  ECalVert->GetXaxis()->SetLabelSize(0.02);
  ECalVert->GetYaxis()->SetLabelSize(0.02);
  ECalVert->GetZaxis()->SetTitle("Charge [pC]");
  ECalVert->GetZaxis()->SetTitleOffset(1.2);

  DrawCrystals(ECalCanv2);

  
  // ECal map with efficiencies from vertical CR
  TCanvas* ECalCanv3 = new TCanvas("eff canv","eff canv",800,800);
  ECalCanv3->cd();
  ECalCanv3->SetLeftMargin(0.05);
  ECalCanv3->SetRightMargin(0.15);
  ECalCanv3->SetBottomMargin(0.05);
  ECalCanv3->SetTopMargin(0.08);
  ECalEff->Draw("colz");
  ECalEff->Draw("text same");
  ECalEff->SetMarkerSize(0.9);
  ECalEff->GetXaxis()->SetNdivisions(29);
  ECalEff->GetYaxis()->SetNdivisions(29);
  ECalEff->GetXaxis()->SetTickLength(0);
  ECalEff->GetYaxis()->SetTickLength(0);
  ECalEff->GetXaxis()->CenterLabels();
  ECalEff->GetYaxis()->CenterLabels();
  ECalEff->GetXaxis()->SetLabelSize(0.02);
  ECalEff->GetYaxis()->SetLabelSize(0.02);
  ECalEff->GetZaxis()->SetTitle("Efficiency [%]");
  ECalEff->GetZaxis()->SetTitleOffset(1.2);

  DrawCrystals(ECalCanv3);

  
  // ECal map with ratio between MPV values from vertical CR
  TCanvas* ECalCanv4 = new TCanvas("ratio canv","ratio canv",800,800);
  ECalCanv4->cd();
  ECalCanv4->SetLeftMargin(0.05);
  ECalCanv4->SetRightMargin(0.15);
  ECalCanv4->SetBottomMargin(0.05);
  ECalCanv4->SetTopMargin(0.08);
  ECalRatio->Draw("colz");
  ECalRatio->Draw("text same");
  ECalRatio->SetMarkerSize(0.9);
  ECalRatio->GetXaxis()->SetNdivisions(29);
  ECalRatio->GetYaxis()->SetNdivisions(29);
  ECalRatio->GetXaxis()->SetTickLength(0);
  ECalRatio->GetYaxis()->SetTickLength(0);
  ECalRatio->GetXaxis()->CenterLabels();
  ECalRatio->GetYaxis()->CenterLabels();
  ECalRatio->GetXaxis()->SetLabelSize(0.02);
  ECalRatio->GetYaxis()->SetLabelSize(0.02);
  ECalRatio->GetZaxis()->SetTitle("Ratio [%]");
  ECalRatio->GetZaxis()->SetTitleOffset(1.2);

  DrawCrystals(ECalCanv4);
  

  if(save){
    string fig_path = "../../cosmics/figures/";
    if(!IsPathExisting("../../cosmics/figures")){
      std::cout<<"\n\nCreating the directory "<<fig_path<<"\n";
      if(mkdir("../../cosmics/figures",S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) cout<<"\nImpossible to create the directory "<<fig_path<<".\n";
    }

    //printf("SAVING FILE NAME: %s%s\n\n",fig_path.c_str(),fname.c_str());

    vertMPVCanv->SaveAs(Form("%s%s_vertCR_MPV_histo.pdf",fig_path.c_str(),fname.c_str()));
    vertMPVCanv->SaveAs(Form("%s%s_vertCR_MPV_histo.eps",fig_path.c_str(),fname.c_str()));
    vertMPVCanv->SaveAs(Form("%s%s_vertCR_MPV_histo.root",fig_path.c_str(),fname.c_str()));

    MPVCanv->SaveAs(Form("%s%s_CR_MPV_histo.pdf",fig_path.c_str(),fname.c_str()));
    MPVCanv->SaveAs(Form("%s%s_CR_MPV_histo.eps",fig_path.c_str(),fname.c_str()));
    MPVCanv->SaveAs(Form("%s%s_CR_MPV_histo.root",fig_path.c_str(),fname.c_str()));

    effCanv->SaveAs(Form("%s%s_Eff_histo.pdf",fig_path.c_str(),fname.c_str()));
    effCanv->SaveAs(Form("%s%s_Eff_histo.eps",fig_path.c_str(),fname.c_str()));
    effCanv->SaveAs(Form("%s%s_Eff_histo.root",fig_path.c_str(),fname.c_str()));

    ECalCanv->SaveAs(Form("%s%s_CR_MPV_map.pdf",fig_path.c_str(),fname.c_str()));
    ECalCanv->SaveAs(Form("%s%s_CR_MPV_map.eps",fig_path.c_str(),fname.c_str()));
    ECalCanv->SaveAs(Form("%s%s_CR_MPV_map.root",fig_path.c_str(),fname.c_str()));

    ECalCanv2->SaveAs(Form("%s%s_vertCR_MPV_map.pdf",fig_path.c_str(),fname.c_str()));
    ECalCanv2->SaveAs(Form("%s%s_vertCR_MPV_map.eps",fig_path.c_str(),fname.c_str()));
    ECalCanv2->SaveAs(Form("%s%s_vertCR_MPV_map.root",fig_path.c_str(),fname.c_str()));

    ECalCanv3->SaveAs(Form("%s%s_Eff_map.pdf",fig_path.c_str(),fname.c_str()));
    ECalCanv3->SaveAs(Form("%s%s_Eff_map.eps",fig_path.c_str(),fname.c_str()));
    ECalCanv3->SaveAs(Form("%s%s_Eff_map.root",fig_path.c_str(),fname.c_str()));

    ECalCanv4->SaveAs(Form("%s%s_MPVRatio_map.pdf",fig_path.c_str(),fname.c_str()));
    ECalCanv4->SaveAs(Form("%s%s_MPVRatio_map.eps",fig_path.c_str(),fname.c_str()));
    ECalCanv4->SaveAs(Form("%s%s_MPVRatio_map.root",fig_path.c_str(),fname.c_str()));
  }

  return;

}


int DoCalculations(char const* dataFile, TCut Cut, char const* idvar,std::map<int, TH1D* >& XYQspec,std::map<int, TH1D* >& XYQspecvert,std::map<int, int >& XY_pass,std::map<int, int >& XY_seen){

  TChain* DataChain = new TChain("NTU");
  DataChain->Add(dataFile);

  Long64_t nentries = DataChain->GetEntries();
  DataChain->SetEstimate(nentries);

  DataChain->SetBranchStatus("*",0);
  DataChain->SetBranchStatus(idvar,1);
  DataChain->SetBranchStatus("IsCR",1);
  DataChain->SetBranchStatus("IsBTF",1);
  DataChain->SetBranchStatus("PosX",1);
  DataChain->SetBranchStatus("PosY",1);


  std::vector<ULong64_t> numbers;
 
  DataChain->Draw(idvar,Cut,"goff");
  double* ev_number = DataChain->GetV1();
  int n_ev = DataChain->GetSelectedRows();

  for(int ii=0;ii<n_ev;++ii){
    ULong64_t number = ULong64_t(ev_number[ii]);
    bool present = false;
    for(size_t ie=0;ie<numbers.size();++ie){
      if(number==numbers[ie]){
	present = true;
	break;
      }
    }
    if(!present){
      numbers.push_back(number);
      //cout<<"Found event "<<number<<endl;
    }
  }
  std::cout<<"\n\nFound "<<numbers.size()<<" events\n"<<std::endl;

  // if there are no events in the selectin skip the file
  std::cout<<"\n\n\n";
  if(numbers.size()==0){
    std::cout<<"No events in selection.\n\n"
	     <<"\t\t!!!FILE WILL BE SKIPPED!!!"<<std::endl;

    TFile *file = DataChain->GetCurrentFile(); 
    DataChain->SetDirectory(0); 
    delete file;

    return 0;
  }

  // enabling needed branches
  //DataChain->SetBranchStatus("*",1);
  DataChain->SetBranchStatus("IsSignal",1);
  DataChain->SetBranchStatus("Board",1);
  DataChain->SetBranchStatus("Qtot",1);
  DataChain->SetBranchStatus("t0",1);
  //DataChain->SetBranchStatus("Npeaks",1);
  //DataChain->SetBranchStatus("Qpulse",1);


  // loop over all the selected events
  for(int je=0;je<int(numbers.size());++je){
    //for(size_t je=0;je<numbers.size();++je){
    TCut DataCut = "IsSignal";
    DataCut += "(Board<10 || (Board>13&&Board<24))";
    DataCut += Cut;
    DataCut += Form("%s==%llu",idvar,numbers[je]);
    // uncomment these lines to avoid the warning
    // Warning in <TTreePlayer::DrawSelect>: The selected TTree subset is empty.
    // but it slows down the macro
    /******
    DataChain->Draw("Npeaks",DataCut,"goff");
    double* npeaks = DataChain->GetV1();
    int nnn = DataChain->GetSelectedRows();
    int skipthis = 0;
    for(int jjc=0;jjc<nnn;++jjc){
      skipthis += int(npeaks[jjc]);
    }
    if(skipthis==0){
      //cout<<"Number "<<numbers[je]<<" will be skipped"<<endl;
      continue;
    }
    *******/

    //DataChain->Draw("PosX:PosY:Qtot",DataCut,"goff");
    if(!(DataChain->Draw("PosX:PosY:Qtot:t0[0]",DataCut,"goff"))) continue;

    double* posX = DataChain->GetV1();
    double* posY = DataChain->GetV2();
    double* qtot = DataChain->GetV3();
    double* t0_0 = DataChain->GetV4();
    int n_cry = DataChain->GetSelectedRows();

    /*
    for(int ic=0;ic<n_cry;++ic){
      std::cout<<"Number "<<numbers[je]<<", X "<<posX[ic]<<", Y "<<posY[ic]<<", Q "<<qtot[ic]<<", t0_0 "<<t0_0[ic]<<std::endl;
    }
    */

    int n_y[29];
    int x_pos[29];// it correctly works only for rows where
                  // there is only one SU w/ signal,
                  // which is what we need
    std::vector<int> xy;
    for(int iy=0;iy<29;++iy) {n_y[iy]=0;}
    for(int iic=0;iic<n_cry;++iic){
      n_y[int(posY[iic])]++;
      x_pos[int(posY[iic])]=int(posX[iic]);
      xy.push_back(int(posX[iic])*100+int(posY[iic]));
    }

    // loop over SUs in the event
    for(int ic=0;ic<n_cry;++ic){

      //// selection of SU with a CR passing vertically ////
      int posXY = int(posX[ic])*100+int(posY[ic]);
      int pinfo = PosInfo(posXY);
      //std::cout<<"PosInfo ("<<posXY<<") = "<<pinfo<<std::endl;

      int pY = int(posY[ic]);
      // selection of vertical CR passing through 3 SUs
      if(n_y[pY]==1){ // only 1 SU in the layer
	int YY1 = -1;
	int YY2 = -1;
	int YY3 = -1;
	if(pinfo==0){ // internal SU
	  YY1=pY; YY2=pY-1; YY3=pY+1;
	  if(PosInfo(posXY-1)==-1){YY2 -= 1;} // the SU below is broken
	  else if(PosInfo(posXY+1)==-1){YY3 += 1;} // the SU above is broken
	}
	else if(pinfo>2){YY1=pY;YY2=pY+1;YY3=pY+2;}
	else if(pinfo>0 && pinfo<3){YY1=pY;YY2=pY-2;YY3=pY-1;}
	if(n_y[YY2]==1 && n_y[YY3]==1){ // only 1 SU in the other 2 layers
	  if(x_pos[YY3]==x_pos[pY] && x_pos[YY2]==x_pos[pY]){ // the 3 SUs are
	                                                        // in column
	    //std::cout<<"ev "<<numbers[je]<<", y "<<posY[ic]<<", x "<<posX[ic]<<std::endl;
	    
	    // check if the histo for that SU is already existing
	    // if not, create it 
	    std::map<int, TH1D* >::iterator xyQv_it;
	    xyQv_it = XYQspecvert.find(posXY);
	    if(xyQv_it==XYQspecvert.end()){
	      XYQspecvert[posXY] = new TH1D(Form("vpos%d",posXY),Form("vpos%d",posXY),500,0.,2000.);
	    } 
	    
	    // fill the SU charge spectrum
	    //XYQspecvert[posXY]->Fill(qtot[ic]);
	    XYQspecvert[posXY]->Fill( qtot[ic]/(1.-(TMath::Exp(-1.*(1000-t0_0[ic])/300.))) );
	    
	  }
	}
      } 
      //// end of selection of vertical CR passing through 3 SUs ////
      

      // check if the spectrum for that SU is already existing
      // if not, create it 
      std::map<int, TH1D* >::iterator xyQ_it;
      xyQ_it = XYQspec.find(posXY);
      if(xyQ_it==XYQspec.end()){
	XYQspec[posXY] = new TH1D(Form("pos%d",posXY),Form("pos%d",posXY),500,0.,2000.);
      } 
      
      // fill the SU charge spectrum
      //XYQspec[posX[ic]*100+posY[ic]]->Fill(qtot[ic]);
      XYQspec[posX[ic]*100+posY[ic]]->Fill( qtot[ic]/(1.-(TMath::Exp(-1.*(1000-t0_0[ic])/300.))) );

      
      /////////
      // efficiency calculation
      if(pY!=0 && pY!=1){ // to avoid out of range in n_y and x_pos
	int yy1=pY, yy2=pY-1, yy3=pY-2;
	if(PosInfo(posXY-1)==-1){yy2-=1;yy3-=1;} // the SU in the midlle is borken
	if(PosInfo(posXY-2)==-1){yy3-=1;} // the bottom SU is borken
	if(n_y[yy1]==1 && n_y[yy3]==1){ // 1 SU per layer
	  if(x_pos[yy1]==x_pos[yy3]){ // top (pY) and bottom (pY-2)
	                              // SU are in column

	    // check if the counters for the SU in the middle 
	    // are already existing. If not, create them
	    std::map<int, int >::iterator xypass_it;
	    xypass_it = XY_pass.find(x_pos[pY]*100+yy2);
	    if(xypass_it==XY_pass.end()){
	      std::cout<<"Inserting maps for "<<x_pos[pY]*100+yy2<<std::endl;
	      XY_pass[x_pos[pY]*100+yy2]=0;
	      XY_seen[x_pos[pY]*100+yy2]=0;
	    }
	    
	    XY_pass[x_pos[pY]*100+yy2]++;

	    std::vector<int>::iterator xy_it;
	    xy_it = find(xy.begin(),xy.end(),x_pos[pY]*100+yy2);
	    if(xy_it!=xy.end()){ // SU in between 2 SUs with signal has signal

	      XY_seen[x_pos[pY]*100+yy2]++;
	      
	      //std::cout<<"ev "<<numbers[je]<<", y "<<yy2<<", x "<<posX[ic]<<", OK"<<std::endl;
	    }

	  }
	} 
      } // end of efficiency calculation 
      /////////

    } // end of loop over SU in the event
    
  } // end of loop over selected events

  TFile *file = DataChain->GetCurrentFile(); 
  DataChain->SetDirectory(0); 
  delete file;

  return 0;
}



void DrawCrystals(TCanvas *Canv){

  Canv->cd();
  
  // central square
  TLine* l1 = new TLine(12,12,17,12);
  l1->Draw("same");
  TLine* l2 = new TLine(12,17,17,17);
  l2->Draw("same");
  TLine* l3 = new TLine(12,12,12,17);
  l3->Draw("same");
  TLine* l4 = new TLine(17,12,17,17);
  l4->Draw("same");

  // vertical lines
  TLine* l5 = new TLine(12,0,17,0);
  l5->Draw("same");
  TLine* l6 = new TLine(12,29,17,29);
  l6->Draw("same");
  TLine* l7 = new TLine(0,12,0,17);
  l7->Draw("same");
  TLine* l8 = new TLine(29,12,29,17);
  l8->Draw("same");

  TLine* l9 = new TLine(12,28,12,29);
  l9->Draw("same");
  TLine* l10 = new TLine(17,28,17,29);
  l10->Draw("same");
  TLine* l11 = new TLine(12,0,12,1);
  l11->Draw("same");
  TLine* l12 = new TLine(17,0,17,1);
  l12->Draw("same");
  
  TLine* l13 = new TLine(9,27,9,28);
  l13->Draw("same");
  TLine* l14 = new TLine(20,27,20,28);
  l14->Draw("same");
  TLine* l15 = new TLine(9,1,9,2);
  l15->Draw("same");
  TLine* l16 = new TLine(20,1,20,2);
  l16->Draw("same");

  TLine* l17 = new TLine(7,26,7,27);
  l17->Draw("same");
  TLine* l18 = new TLine(22,26,22,27);
  l18->Draw("same");
  TLine* l19 = new TLine(7,2,7,3);
  l19->Draw("same");
  TLine* l20 = new TLine(22,2,22,3);
  l20->Draw("same");

  TLine* l21 = new TLine(24,25,24,26);
  l21->Draw("same");
  TLine* l22 = new TLine(5,25,5,26);
  l22->Draw("same");
  TLine* l23 = new TLine(24,3,24,4);
  l23->Draw("same");
  TLine* l24 = new TLine(5,3,5,4);
  l24->Draw("same");

  TLine* l25 = new TLine(25,24,25,25);
  l25->Draw("same");
  TLine* l26 = new TLine(4,24,4,25);
  l26->Draw("same");
  TLine* l27 = new TLine(25,4,25,5);
  l27->Draw("same");
  TLine* l28 = new TLine(4,4,4,5);
  l28->Draw("same");

  TLine* l29 = new TLine(26,22,26,24);
  l29->Draw("same");
  TLine* l30 = new TLine(3,22,3,24);
  l30->Draw("same");
  TLine* l31 = new TLine(26,5,26,7);
  l31->Draw("same");
  TLine* l32 = new TLine(3,5,3,7);
  l32->Draw("same");

  TLine* l33 = new TLine(27,20,27,22);
  l33->Draw("same");
  TLine* l34 = new TLine(2,20,2,22);
  l34->Draw("same");
  TLine* l35 = new TLine(27,7,27,9);
  l35->Draw("same");
  TLine* l36 = new TLine(2,7,2,9);
  l36->Draw("same");

  TLine* l37 = new TLine(28,17,28,20);
  l37->Draw("same");
  TLine* l38 = new TLine(1,17,1,20);
  l38->Draw("same");
  TLine* l39 = new TLine(28,9,28,12);
  l39->Draw("same");
  TLine* l40 = new TLine(1,9,1,12);
  l40->Draw("same");

  // horizontal lines
  /*
  TLine* l41 = new TLine(0,12,0,17);
  l41->Draw("same");
  TLine* l42 = new TLine(29,12,29,17);
  l42->Draw("same");
  TLine* l43 = new TLine(12,0,17,0);
  l43->Draw("same");
  TLine* l44 = new TLine(12,29,17,29);
  l44->Draw("same");
  */
  TLine* l45 = new TLine(28,12,29,12);
  l45->Draw("same");
  TLine* l46 = new TLine(28,17,29,17);
  l46->Draw("same");
  TLine* l47 = new TLine(0,12,1,12);
  l47->Draw("same");
  TLine* l48 = new TLine(0,17,1,17);
  l48->Draw("same");
  
  TLine* l49 = new TLine(27,9,28,9);
  l49->Draw("same");
  TLine* l50 = new TLine(27,20,28,20);
  l50->Draw("same");
  TLine* l51 = new TLine(1,9,2,9);
  l51->Draw("same");
  TLine* l52 = new TLine(1,20,2,20);
  l52->Draw("same");

  TLine* l53 = new TLine(26,7,27,7);
  l53->Draw("same");
  TLine* l54 = new TLine(26,22,27,22);
  l54->Draw("same");
  TLine* l55 = new TLine(2,7,3,7);
  l55->Draw("same");
  TLine* l56 = new TLine(2,22,3,22);
  l56->Draw("same");

  TLine* l57 = new TLine(25,24,26,24);
  l57->Draw("same");
  TLine* l58 = new TLine(25,5,26,5);
  l58->Draw("same");
  TLine* l59 = new TLine(3,24,4,24);
  l59->Draw("same");
  TLine* l60 = new TLine(3,5,4,5);
  l60->Draw("same");

  TLine* l61 = new TLine(24,25,25,25);
  l61->Draw("same");
  TLine* l62 = new TLine(24,4,25,4);
  l62->Draw("same");
  TLine* l63 = new TLine(4,25,5,25);
  l63->Draw("same");
  TLine* l64 = new TLine(4,4,5,4);
  l64->Draw("same");

  TLine* l65 = new TLine(22,26,24,26);
  l65->Draw("same");
  TLine* l66 = new TLine(22,3,24,3);
  l66->Draw("same");
  TLine* l67 = new TLine(5,26,7,26);
  l67->Draw("same");
  TLine* l68 = new TLine(5,3,7,3);
  l68->Draw("same");

  TLine* l69 = new TLine(20,27,22,27);
  l69->Draw("same");
  TLine* l70 = new TLine(20,2,22,2);
  l70->Draw("same");
  TLine* l71 = new TLine(7,27,9,27);
  l71->Draw("same");
  TLine* l72 = new TLine(7,2,9,2);
  l72->Draw("same");

  TLine* l73 = new TLine(17,28,20,28);
  l73->Draw("same");
  TLine* l74 = new TLine(17,1,20,1);
  l74->Draw("same");
  TLine* l75 = new TLine(9,28,12,28);
  l75->Draw("same");
  TLine* l76 = new TLine(9,1,12,1);
  l76->Draw("same");

  TLine* l77 = new TLine(9,1,12,1);
  l77->Draw("same");
  TLine* l78 = new TLine(9,1,12,1);
  l78->Draw("same");
  TLine* l79 = new TLine(9,1,12,1);
  l79->Draw("same");
  TLine* l80 = new TLine(9,1,12,1);
  l80->Draw("same");

  l1->SetLineWidth(2);
  l2->SetLineWidth(2);
  l3->SetLineWidth(2);
  l4->SetLineWidth(2);
  l5->SetLineWidth(2);
  l6->SetLineWidth(2);
  l7->SetLineWidth(2);
  l8->SetLineWidth(2);
  l9->SetLineWidth(2);
  l10->SetLineWidth(2);
  l11->SetLineWidth(2);
  l12->SetLineWidth(2);
  l13->SetLineWidth(2);
  l14->SetLineWidth(2);
  l15->SetLineWidth(2);
  l16->SetLineWidth(2);
  l17->SetLineWidth(2);
  l18->SetLineWidth(2);
  l19->SetLineWidth(2);
  l20->SetLineWidth(2);
  l21->SetLineWidth(2);
  l22->SetLineWidth(2);
  l23->SetLineWidth(2);
  l24->SetLineWidth(2);
  l25->SetLineWidth(2);
  l26->SetLineWidth(2);
  l27->SetLineWidth(2);
  l28->SetLineWidth(2);
  l29->SetLineWidth(2);
  l30->SetLineWidth(2);
  l31->SetLineWidth(2);
  l32->SetLineWidth(2);
  l33->SetLineWidth(2);
  l34->SetLineWidth(2);
  l35->SetLineWidth(2);
  l36->SetLineWidth(2);
  l37->SetLineWidth(2);
  l38->SetLineWidth(2);
  l39->SetLineWidth(2);
  l40->SetLineWidth(2);
  //l41->SetLineWidth(2);
  //l42->SetLineWidth(2);
  //l43->SetLineWidth(2);
  //l44->SetLineWidth(2);
  l45->SetLineWidth(2);
  l46->SetLineWidth(2);
  l47->SetLineWidth(2);
  l48->SetLineWidth(2);
  l49->SetLineWidth(2);
  l50->SetLineWidth(2);
  l51->SetLineWidth(2);
  l52->SetLineWidth(2);
  l53->SetLineWidth(2);
  l54->SetLineWidth(2);
  l55->SetLineWidth(2);
  l56->SetLineWidth(2);
  l57->SetLineWidth(2);
  l58->SetLineWidth(2);
  l59->SetLineWidth(2);
  l60->SetLineWidth(2);
  l61->SetLineWidth(2);
  l62->SetLineWidth(2);
  l63->SetLineWidth(2);
  l64->SetLineWidth(2);
  l65->SetLineWidth(2);
  l66->SetLineWidth(2);
  l67->SetLineWidth(2);
  l68->SetLineWidth(2);
  l69->SetLineWidth(2);
  l70->SetLineWidth(2);
  l71->SetLineWidth(2);
  l72->SetLineWidth(2);
  l73->SetLineWidth(2);
  l74->SetLineWidth(2);
  l75->SetLineWidth(2);
  l76->SetLineWidth(2);
  l77->SetLineWidth(2);
  l78->SetLineWidth(2);
  l79->SetLineWidth(2);
  l80->SetLineWidth(2);


  // grid x
  for(int ix=1;ix<30;++ix){
    TLine* lix = new TLine(ix,0,ix,29);
    lix->SetLineStyle(3);
    lix->Draw("same");
  }  
  // grid y
  for(int iy=1;iy<30;++iy){
    TLine* liy = new TLine(0,iy,29,iy);
    liy->SetLineStyle(3);
    liy->Draw("same");
  }  

}



string GetFileName(string path){

  std::string lin = path;
  std::size_t begin, end;
  
  begin = lin.find_last_of("/");
  if(begin != std::string::npos){
    lin.erase(0,begin+1);
  }

  begin = lin.find("CR_")+3;
  if(begin != std::string::npos){
    lin.erase(0,begin);
  }

  end = lin.find_last_of(".");
  if(begin != std::string::npos){
    lin.erase(end);
  } 

  return lin;	
  
}



int PosInfo(int x_y){

  int xx = x_y/100;
  int yy = x_y%100;


  std::map<int, vector<int> > y_xbord;

  std::vector<int> y0{12,13,14,15,16};
  y_xbord[0] = y0;
  std::vector<int> y1{9,10,11,17,18,19};
  y_xbord[1] = y1;
  std::vector<int> y2{7,8,20,21};
  y_xbord[2] = y2;
  std::vector<int> y3{5,6,22,23};
  y_xbord[3] = y3;
  std::vector<int> y4{4,24};
  y_xbord[4] = y4;
  std::vector<int> y5{3,25};
  y_xbord[5] = y5;
  std::vector<int> y7{2,26};
  y_xbord[7] = y7;
  std::vector<int> y9{1,27};
  y_xbord[9] = y9;
  std::vector<int> y11{12,13,14,15,16};// inner square
  y_xbord[11] = y11;// inner square
  std::vector<int> y12{0,28};
  y_xbord[12] = y12;
  std::vector<int> y16{0,28};
  y_xbord[16] = y16;
  std::vector<int> y17{12,13,14,15,16};// inner square
  y_xbord[17] = y17;// inner square
  std::vector<int> y19{1,27};
  y_xbord[19] = y19;
  std::vector<int> y21{2,26};
  y_xbord[21] = y21;
  std::vector<int> y23{3,25};
  y_xbord[23] = y23;
  std::vector<int> y24{4,24};
  y_xbord[24] = y24;
  std::vector<int> y25{5,6,22,23};
  y_xbord[25] = y25;
  std::vector<int> y26{7,8,20,21};
  y_xbord[26] = y26;
  std::vector<int> y27{9,10,11,17,18,19};
  y_xbord[27] = y27;
  std::vector<int> y28{12,13,14,15,16};
  y_xbord[28] = y28;


  std::map<int, std::vector<int> >::iterator yx_it;
  yx_it = y_xbord.find(yy);
  if(yx_it!=y_xbord.end()){
    for(unsigned int iel=0;iel<(yx_it->second).size();++iel){
      if(xx==(yx_it->second)[iel]){

	// inner square upper edge
	if(yy==11)
	return 2;

	// inner square lower edge
	else if(yy==17)
	  return 3;

	// upper ECal half
	else if(yy>14)
	  return 1;

	// lower ECal half
	else if(yy<14)
	  return 4;
      }

    }

  }

  // if it is a non working channel return 5
  if((xx==16&&yy==25) || (xx==18&&yy==4) || (xx==18&&yy==10) || (xx==22&&yy==8))
    return -1;

  // if non of these cases return 0
  return 0;

}




bool IsPathExisting(const std::string &s){

  struct stat buffer;

  return (stat (s.c_str(), &buffer) == 0);
}
