import FWCore.ParameterSet.Config as cms

def eGammaCorrection(
    process,
    electronCollection,
    photonCollection,
    corElectronCollection,
    corPhotonCollection,
    metCollections,
    pfCandMatching=False,
    pfCandidateCollection="",
    postfix=""
):

    process.load("PhysicsTools.PatAlgos.cleaningLayer1.photonCleaner_cfi")
    #cleaning the bad collections
    cleanedPhotonCollection="cleanedPhotons"+postfix
    cleanPhotonProducer = getattr(process, "cleanPatPhotons").clone( 
        src = photonCollection,
    )
    cleanPhotonProducer.checkOverlaps.electrons.src = electronCollection
    cleanPhotonProducer.checkOverlaps.electrons.requireNoOverlaps=cms.bool(True)
    
    #cleaning the good collections
    cleanedCorPhotonCollection="cleanedCorPhotons"+postfix
    cleanCorPhotonProducer = getattr(process, "cleanPatPhotons").clone( 
        src = corPhotonCollection
    )
    cleanCorPhotonProducer.checkOverlaps.electrons.src = corElectronCollection
    cleanCorPhotonProducer.checkOverlaps.electrons.requireNoOverlaps=cms.bool(True)
    #cleanCorPhotonProducer = cms.EDProducer("CandPtrProjector", 
    #src = cms.InputTag(corPhotonCollection),
    #veto = cms.InputTag(corElectronCollection)
    #)


    #matching between objects
    matchPhotonCollection="matchedPhotons"+postfix
    matchPhotonProducer=cms.EDProducer("PFMatchedCandidateRefExtractor",
        col1=cms.InputTag(cleanedPhotonCollection),
        col2=cms.InputTag(cleanedCorPhotonCollection),
        pfCandCollection=cms.InputTag(pfCandidateCollection),
        extractPFCandidates=cms.bool(pfCandMatching) )
    
    matchElectronCollection="matchedElectrons"+postfix
    matchElectronProducer=cms.EDProducer("PFMatchedCandidateRefExtractor",
        col1=cms.InputTag(electronCollection),
        col2=cms.InputTag(corElectronCollection),
        pfCandCollection=cms.InputTag(pfCandidateCollection),
        extractPFCandidates=cms.bool(pfCandMatching) )
    

    #removal of old objects, and replace by the new 
    tag1= "pfCandCol1" if pfCandMatching else "col1"
    tag2= "pfCandCol2" if pfCandMatching else "col2"
    correctionPhoton="corMETPhoton"+postfix
    corMETPhoton = cms.EDProducer("ShiftedParticleMETcorrInputProducer",
        srcOriginal = cms.InputTag(matchPhotonCollection,tag1),
        srcShifted = cms.InputTag(matchPhotonCollection,tag2),
    )
    correctionElectron="corMETElectron"+postfix
    corMETElectron=cms.EDProducer("ShiftedParticleMETcorrInputProducer",
        srcOriginal=cms.InputTag(matchElectronCollection,tag1),
        srcShifted=cms.InputTag(matchElectronCollection,tag2),
    )


    setattr(process,cleanedPhotonCollection,cleanPhotonProducer)
    setattr(process,cleanedCorPhotonCollection,cleanCorPhotonProducer)
    setattr(process,matchPhotonCollection,matchPhotonProducer)
    setattr(process,matchElectronCollection,matchElectronProducer)
    setattr(process,correctionPhoton,corMETPhoton)
    setattr(process,correctionElectron,corMETElectron)

    sequence=cms.Sequence()
    sequence+=getattr(process,cleanedPhotonCollection)
    sequence+=getattr(process,cleanedCorPhotonCollection)
    # BUG IN ORIGINAL CODE: match(Photon|Electron)Collection not added to sequence
    sequence+=getattr(process,matchPhotonCollection)
    sequence+=getattr(process,matchElectronCollection)
    sequence+=getattr(process,correctionPhoton)
    sequence+=getattr(process,correctionElectron)


    
    #MET corrector
    for metCollection in metCollections:
        #print "---------->>>> ",metCollection, postfix
        if not hasattr(process, metCollection+postfix):
            #print " ==>> aqui"
            #raw met is the only one that does not have already a valid input collection
            # WHY MAKE IT LOOK LIKE IT HANDLES GENERAL CASES THEN??

            # BUG IN THE ORIGINAL CODE: was replacing from metCollection
            inputMetCollection = (metCollection + postfix).replace("Raw","",1) 
            corMETModule = cms.EDProducer("CorrectedPATMETProducer",
                src = cms.InputTag( inputMetCollection ),
                #"patPFMet" if ("Raw" in metCollection )else metCollection
                srcCorrections = cms.VInputTag(
                    cms.InputTag(correctionPhoton),
                    cms.InputTag(correctionElectron),
                )
            )
            setattr(process,metCollection+postfix,corMETModule)
            sequence+=getattr(process,metCollection+postfix)
        else:
            # BUG IN THE ORIGINAL CODE: was setting attrs of metCollection
            getattr(process,metCollection + postfix).srcCorrections.append(cms.InputTag(correctionPhoton))
            getattr(process,metCollection + postfix).srcCorrections.append(cms.InputTag(correctionElectron))
            
    return sequence
