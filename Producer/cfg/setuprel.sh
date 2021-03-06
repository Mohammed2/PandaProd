#!/bin/bash

# Use PandaProd/Producer/scripts/cms-git-diff to find out which packages should be checked out from each branch.

CWD=`pwd`
SRC=$CMSSW_BASE/src
INSTALL=$CMSSW_BASE/src/PandaProd/Producer/scripts/install-pkg


# Cut-based electron and photon ID
$INSTALL lsoffi CMSSW_9_4_0_pre3_TnP RecoEgamma/ElectronIdentification RecoEgamma/PhotonIdentification

# MVA electron ID
# RecoEgamma/ElectronIdentification overlaps with the line above but most of the files added above are not contained in this branch and thus will not be overwritten
$INSTALL guitargeek ElectronID_MVA2017_940pre3 RecoEgamma/EgammaTools RecoEgamma/ElectronIdentification -x RecoEgamma/ElectronIdentification/python/Identification/cutBasedElectronID_tools.py
# This is getting ridiculous (difference between 94X and 101X is purely technical)
cp $CMSSW_RELEASE_BASE/src/RecoEgamma/EgammaTools/src/EcalClusterLocal.cc $SRC/RecoEgamma/EgammaTools/src/EcalClusterLocal.cc

# MVA weights for ID
git clone -b CMSSW_9_4_0_pre3_TnP https://github.com/lsoffi/RecoEgamma-ElectronIdentification.git electronid
cp -r electronid/* $SRC/RecoEgamma/ElectronIdentification/data/
rm -rf electronid
git clone -b CMSSW_9_4_0_pre3_TnP https://github.com/lsoffi/RecoEgamma-PhotonIdentification.git photonid
cp -r photonid/* $SRC/RecoEgamma/PhotonIdentification/data/
rm -rf photonid

# Electron and photon calibration
$INSTALL cms-egamma EGM_94X_v1 EgammaAnalysis/ElectronTools

# MVA weights
cd $SRC/EgammaAnalysis/ElectronTools/data
git clone https://github.com/ECALELFS/ScalesSmearings.git
cd ScalesSmearings/
git checkout Run2017_17Nov2017_v1
cd $CWD

# Jet & MET reclustering (maybe not strictly needed in our current prod.py because we don't recluster)
$INSTALL cms-met METRecipe94x PhysicsTools/PatAlgos PhysicsTools/PatUtils

# Deep double B tagger from FNAL
#$INSTALL jmduarte double-b-rebased-94x  RecoBTag/Combined RecoBTag/Configuration RecoBTag/DeepFlavour DataFormats/BTauReco PhysicsTools/PatAlgos
$INSTALL DylanHsu double-b-rebased-94x  RecoBTag/Combined RecoBTag/Configuration RecoBTag/DeepFlavour DataFormats/BTauReco PhysicsTools/PatAlgos

cp $CMSSW_RELEASE_BASE/src/PhysicsTools/PatAlgos/plugins/PATElectronProducer.h $SRC/PhysicsTools/PatAlgos/plugins/PATElectronProducer.h
cp $CMSSW_RELEASE_BASE/src/PhysicsTools/PatAlgos/plugins/PATElectronProducer.cc $SRC/PhysicsTools/PatAlgos/plugins/PATElectronProducer.cc
cp $CMSSW_RELEASE_BASE/src/PhysicsTools/PatAlgos/plugins/PATPhotonProducer.cc $SRC/PhysicsTools/PatAlgos/plugins/PATPhotonProducer.cc
cp $CMSSW_RELEASE_BASE/src/PhysicsTools/PatAlgos/plugins/PATElectronSlimmer.cc $SRC/PhysicsTools/PatAlgos/plugins/PATElectronSlimmer.cc
cp $CMSSW_RELEASE_BASE/src/PhysicsTools/PatAlgos/plugins/PATCompositeCandidateProducer.cc $SRC/PhysicsTools/PatAlgos/plugins/PATCompositeCandidateProducer.cc

# clone to a temp area and move the contents to avoid including .git
TEMP=$(mktemp -d)
git clone https://github.com/jmduarte/RecoBTag-Combined.git $TEMP/data -b deepdoubleb_v0
mkdir -p $SRC/RecoBTag/Combined/data
mv $TEMP/data/DeepDoubleB $SRC/RecoBTag/Combined/data/
rm -rf $TEMP
