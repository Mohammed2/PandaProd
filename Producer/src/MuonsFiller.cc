#include "../interface/MuonsFiller.h"

#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/MuonReco/interface/MuonSelectors.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/PatCandidates/interface/TriggerObjectStandAlone.h"
#include "DataFormats/PatCandidates/interface/PackedCandidate.h"
#include "DataFormats/Common/interface/RefToPtr.h"

MuonsFiller::MuonsFiller(std::string const& _name, edm::ParameterSet const& _cfg, edm::ConsumesCollector& _coll) :
  FillerBase(_name, _cfg)
{
  getToken_(muonsToken_, _cfg, _coll, "muons");
  getToken_(verticesToken_, _cfg, _coll, "common", "vertices");

  if (useTrigger_) {
    for (unsigned iT(0); iT != panda::Muon::nTriggerObjects; ++iT) {
      std::string name(panda::Muon::TriggerObjectName[iT]); // "f<trigger filter name>"
      auto filters(getParameter_<VString>(_cfg, "triggerObjects." + name.substr(1)));
      triggerObjectNames_[iT].insert(filters.begin(), filters.end());
    }
  }
}

void
MuonsFiller::addOutput(TFile& _outputFile)
{
  TDirectory::TContext context(&_outputFile);
  auto* t(panda::utils::makeDocTree("MuonTriggerObject", panda::Muon::TriggerObjectName, panda::Muon::nTriggerObjects));
  t->Write();
  delete t;
}

void
MuonsFiller::branchNames(panda::utils::BranchList& _eventBranches, panda::utils::BranchList&) const
{
  _eventBranches.emplace_back("muons");

  if (isRealData_)
    _eventBranches.emplace_back("!muons.matchedGen_");
  if (!useTrigger_)
    _eventBranches.emplace_back("!muons.triggerMatch");
}

void
MuonsFiller::fill(panda::Event& _outEvent, edm::Event const& _inEvent, edm::EventSetup const& _setup)
{
  auto& inMuons(getProduct_(_inEvent, muonsToken_));
  auto& vertices(getProduct_(_inEvent, verticesToken_));

  auto& outMuons(_outEvent.muons);

  std::vector<edm::Ptr<reco::Muon>> ptrList;

  unsigned iMu(-1);
  for (auto& inMuon : inMuons) {
    ++iMu;
    auto& outMuon(outMuons.create_back());

    fillP4(outMuon, inMuon);

    outMuon.global = inMuon.isGlobalMuon();
    outMuon.tracker = inMuon.isTrackerMuon();
    outMuon.pf = inMuon.isPFMuon();
    
    auto&& innerTrack(inMuon.innerTrack());
    if (innerTrack.isNonnull()) {
      outMuon.validFraction = innerTrack->validFraction();
      auto&& hitPattern(innerTrack->hitPattern());
      outMuon.trkLayersWithMmt = hitPattern.trackerLayersWithMeasurement();
      outMuon.pixLayersWithMmt = hitPattern.pixelLayersWithMeasurement();
      outMuon.nValidPixel = hitPattern.numberOfValidPixelHits();
    }

    auto&& globalTrack(inMuon.globalTrack());
    if (globalTrack.isNonnull()) {
      outMuon.normChi2 = globalTrack->normalizedChi2();
      auto&& hitPattern(globalTrack->hitPattern());
      outMuon.nValidMuon = hitPattern.numberOfValidMuonHits();
    }

    outMuon.nMatched = inMuon.numberOfMatchedStations();

    auto&& combQuality(inMuon.combinedQuality());
    outMuon.chi2LocalPosition = combQuality.chi2LocalPosition;
    outMuon.trkKink = combQuality.trkKink;

    outMuon.segmentCompatibility = muon::segmentCompatibility(inMuon);

    outMuon.charge = inMuon.charge();

    auto& pfIso(inMuon.pfIsolationR04());

    outMuon.chIso = pfIso.sumChargedHadronPt;
    outMuon.nhIso = pfIso.sumNeutralHadronEt;
    outMuon.phIso = pfIso.sumPhotonEt;
    outMuon.puIso = pfIso.sumPUPt;
    outMuon.r03Iso = inMuon.isolationR03().sumPt;

    auto* patMuon(dynamic_cast<pat::Muon const*>(&inMuon));

    if (patMuon) {
      outMuon.loose = patMuon->isLooseMuon();
      outMuon.medium = patMuon->isMediumMuon();
      // Following the "short-term instruction for Moriond 2017" given in https://twiki.cern.ch/twiki/bin/viewauth/CMS/SWGuideMuonIdRun2#MediumID2016_to_be_used_with_Run
      // Valid only for runs B-F
      outMuon.mediumBtoF = outMuon.loose && inMuon.innerTrack()->validFraction() > 0.49 &&
        ((inMuon.isGlobalMuon() &&
          inMuon.globalTrack()->normalizedChi2() < 3. &&
          inMuon.combinedQuality().chi2LocalPosition < 12. &&
          inMuon.combinedQuality().trkKink < 20. &&
          muon::segmentCompatibility(inMuon) > 0.303) ||
         muon::segmentCompatibility(inMuon) > 0.451);
          
      if (vertices.size() == 0) {
        outMuon.tight = false;
        outMuon.soft = false;
      }
      else {
        outMuon.tight = patMuon->isTightMuon(vertices.at(0));
        outMuon.soft = patMuon->isSoftMuon(vertices.at(0));
      }
    }
    else {
      outMuon.loose = muon::isLooseMuon(inMuon);
      outMuon.medium = muon::isMediumMuon(inMuon);
      if (vertices.size() == 0) {
        outMuon.tight = false;
        outMuon.soft = false;
      }
      else {
        outMuon.tight = muon::isTightMuon(inMuon, vertices.at(0));
        outMuon.soft = muon::isSoftMuon(inMuon, vertices.at(0));
      }
    }

    outMuon.hltsafe = outMuon.combIso() / outMuon.pt() < 0.4 && outMuon.r03Iso / outMuon.pt() < 0.4;

    auto bestTrack(inMuon.muonBestTrack());
    if (vertices.size() != 0) {
      auto& pv(vertices.at(0));
      auto pos(pv.position());
      if (patMuon)
        outMuon.dxy = patMuon->dB(); // probably gives identical value as bestTrack->dxy()
      else
        outMuon.dxy = std::abs(bestTrack->dxy(pos));

      outMuon.dz = std::abs(bestTrack->dz(pos));
    }
    else {
      if (patMuon)
        outMuon.dxy = patMuon->dB(); // probably gives identical value as bestTrack->dxy()
      else
        outMuon.dxy = std::abs(bestTrack->dxy());

      outMuon.dz = std::abs(bestTrack->dz());
    }

    outMuon.pfPt = inMuon.pfP4().pt();

    ptrList.push_back(inMuons.ptrAt(iMu));
  }
  
  auto originalIndices(outMuons.sort(panda::Particle::PtGreater));

  // export panda <-> reco mapping

  auto& muMuMap(objectMap_->get<reco::Muon, panda::Muon>());
  auto& pfMuMap(objectMap_->get<reco::Candidate, panda::Muon>("pf"));
  auto& vtxMuMap(objectMap_->get<reco::Vertex, panda::Muon>());
  auto& genMuMap(objectMap_->get<reco::Candidate, panda::Muon>("gen"));

  for (unsigned iP(0); iP != outMuons.size(); ++iP) {
    auto& outMuon(outMuons[iP]);
    unsigned idx(originalIndices[iP]);
    muMuMap.add(ptrList[idx], outMuon);

    auto sourcePtr(ptrList[idx]->sourceCandidatePtr(0));
    if (sourcePtr.isNonnull()) {
      pfMuMap.add(sourcePtr, outMuon);
      if (dynamic_cast<pat::PackedCandidate const*>(sourcePtr.get())) {
        auto vtxRef(static_cast<pat::PackedCandidate const&>(*sourcePtr).vertexRef());
        if (vtxRef.isNonnull())
          vtxMuMap.add(edm::refToPtr(vtxRef), outMuon);
      }
    }

    if (!isRealData_) {
      auto& inMuon(*ptrList[idx]);

      if (dynamic_cast<pat::Muon const*>(&inMuon)) {
        auto& patMuon(static_cast<pat::Muon const&>(inMuon));
        auto ref(patMuon.genParticleRef());
        if (ref.isNonnull())
          genMuMap.add(edm::refToPtr(ref), outMuon);
      }
    }
  }
}

void
MuonsFiller::setRefs(ObjectMapStore const& _objectMaps)
{
  auto& pfMuMap(objectMap_->get<reco::Candidate, panda::Muon>("pf"));
  auto& vtxMuMap(objectMap_->get<reco::Vertex, panda::Muon>());

  auto& pfMap(_objectMaps.at("pfCandidates").get<reco::Candidate, panda::PFCand>().fwdMap);
  auto& vtxMap(_objectMaps.at("vertices").get<reco::Vertex, panda::RecoVertex>().fwdMap);

  for (auto& link : pfMuMap.bwdMap) { // panda -> edm
    auto& outMuon(*link.first);
    auto& pfPtr(link.second);

    // muon sourceCandidatePtr can point to the AOD pfCandidates in some cases
    auto pfItr(pfMap.find(pfPtr));
    if (pfItr == pfMap.end())
      continue;

    outMuon.matchedPF.setRef(pfItr->second);
  }

  for (auto& link : vtxMuMap.bwdMap) { // panda -> edm
    auto& outMuon(*link.first);
    auto& vtxPtr(link.second);

    outMuon.vertex.setRef(vtxMap.at(vtxPtr));
  }

  if (!isRealData_) {
    auto& genMuMap(objectMap_->get<reco::Candidate, panda::Muon>("gen"));

    auto& genMap(_objectMaps.at("genParticles").get<reco::Candidate, panda::GenParticle>().fwdMap);

    for (auto& link : genMuMap.bwdMap) {
      auto& genPtr(link.second);
      if (genMap.find(genPtr) == genMap.end())
        continue;

      auto& outMuon(*link.first);
      outMuon.matchedGen.setRef(genMap.at(genPtr));
    }
  }

  if (useTrigger_) {
    auto& nameMap(_objectMaps.at("hlt").get<pat::TriggerObjectStandAlone, VString>().fwdMap);

    std::vector<pat::TriggerObjectStandAlone const*> triggerObjects[panda::Muon::nTriggerObjects];

    // loop over all trigger objects
    for (auto& mapEntry : nameMap) { // (pat object, list of filter names)
      // loop over the trigger filters we are interested in
      for (unsigned iT(0); iT != panda::Muon::nTriggerObjects; ++iT) {
        // each triggerObjectNames_[] can have multiple filters
        for (auto& name : triggerObjectNames_[iT]) {
          auto nItr(std::find(mapEntry.second->begin(), mapEntry.second->end(), name));
          if (nItr != mapEntry.second->end()) {
            triggerObjects[iT].push_back(mapEntry.first.get());
            break;
          }
        }
      }
    }

    auto& muMuMap(objectMap_->get<reco::Muon, panda::Muon>().fwdMap);

    for (auto& link : muMuMap) { // edm -> panda
      auto& inMuon(*link.first);
      auto& outMuon(*link.second);

      for (unsigned iT(0); iT != panda::Muon::nTriggerObjects; ++iT) {
        for (auto* obj : triggerObjects[iT]) {
          if (reco::deltaR(*obj, inMuon) < 0.3) {
            outMuon.triggerMatch[iT] = true;
            break;
          }
        }
      }
    }
  }
}

DEFINE_TREEFILLER(MuonsFiller);
