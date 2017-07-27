#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "EventFilter/L1TRawToDigi/plugins/UnpackerFactory.h"

#include "L1Trigger/L1TMuon/interface/MuonRawDigiTranslator.h"

#include "MuonUnpacker.h"

namespace l1t {
   namespace stage2 {
      MuonUnpacker::MuonUnpacker() : algoVersion_(0), muonCopy_(0)
      {
      }

      bool
      MuonUnpacker::unpack(const Block& block, UnpackerCollections *coll)
      {
         LogDebug("L1T") << "Block ID  = " << block.header().getID() << " size = " << block.header().getSize();

         auto payload = block.payload();

         int nBX, firstBX, lastBX;
         // Check if per BX zero suppression was enabled
         bool bxZsEnabled = ((block.header().getFlags() >> bxzs_enable_shift_) & 0x1) == 1;
         // Calculate the total number of BXs
         if (bxZsEnabled) {
            BxBlockHeader bxHeader(payload.at(0));
            nBX = bxHeader.getTotalBx();
         } else {
            nBX = int(ceil(block.header().getSize() / nWords_));
         }
         getBXRange(nBX, firstBX, lastBX);

         // Create the muon collection and set the BX range
         auto res = static_cast<L1TObjectCollections*>(coll)->getMuons(muonCopy_);
         res->setBXRange(firstBX, lastBX);
         LogDebug("L1T") << "nBX = " << nBX << " first BX = " << firstBX << " lastBX = " << lastBX;

         // Get the BX blocks and unpack them
         auto bxBlocks = block.getBxBlocks(nWords_, bxZsEnabled);
         for (const auto& bxBlock : bxBlocks) {
            unpackBx(res, bxBlock.header().getBx(), bxBlock.payload());
         }
         return true;
      }


      void
      MuonUnpacker::unpackBx(MuonBxCollection* res, int bx, const std::vector<uint32_t>& payload, unsigned int startIdx)
      {
         unsigned int i = startIdx;
         // Check if there are enough words left in the payload
         if (i + nWords_ <= payload.size()) {
            for (unsigned nWord = 0; nWord < nWords_; nWord += 2) {
               uint32_t raw_data_00_31 = payload[i++];
               uint32_t raw_data_32_63 = payload[i++];        
               LogDebug("L1T") << "raw_data_00_31 = 0x" << hex << raw_data_00_31 << " raw_data_32_63 = 0x" << raw_data_32_63;
               // skip empty muons (hwPt == 0)
               if (((raw_data_00_31 >> l1t::MuonRawDigiTranslator::ptShift_) & l1t::MuonRawDigiTranslator::ptMask_) == 0) {
                  LogDebug("L1T") << "Muon hwPt zero. Skip.";
                  continue;
               }

               Muon mu;
                   
               MuonRawDigiTranslator::fillMuon(mu, raw_data_00_31, raw_data_32_63, fed_, algoVersion_);

               LogDebug("L1T") << "Mu" << nWord/2 << ": eta " << mu.hwEta() << " phi " << mu.hwPhi() << " pT " << mu.hwPt() << " iso " << mu.hwIso() << " qual " << mu.hwQual() << " charge " << mu.hwCharge() << " charge valid " << mu.hwChargeValid();

               res->push_back(bx, mu);
            }
         } else {
            edm::LogWarning("L1T") << "Only " << payload.size() - i << " 32 bit words in this BX but " << nWords_ << " are required. Not unpacking the data for BX " << bx << ".";
         }
      }
   }
}

DEFINE_L1T_UNPACKER(l1t::stage2::MuonUnpacker);
