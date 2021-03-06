// nnet/nnet-uni-fsmn.h

// Copyright 2018 Alibaba.Inc (Author: Shiliang Zhang) 

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.


#ifndef KALDI_NNET_NNET_UNI_FSMN_H_
#define KALDI_NNET_NNET_UNI_FSMN_H_


#include "nnet/nnet-component.h"
#include "nnet/nnet-utils.h"
#include "cudamatrix/cu-math.h"
#include "cudamatrix/cu-kernels.h"


namespace kaldi {
namespace nnet1 {
 class UniFsmn : public UpdatableComponent {
  public:
   UniFsmn(int32 dim_in, int32 dim_out)
     : UpdatableComponent(dim_in, dim_out),
     learn_rate_coef_(1.0)
   {
   }
   ~UniFsmn()
   { }

   Component* Copy() const { return new UniFsmn(*this); }
   ComponentType GetType() const { return kUniFsmn; }

   void SetFlags(const Vector<BaseFloat> &flags) {
     flags_.Resize(flags.Dim(), kSetZero);
     flags_.CopyFromVec(flags);
   }
   void InitData(std::istream                                                     &is) {
     // define options
     float learn_rate_coef = 1.0;
     int l_order = 1;
     int l_stride = 1;
     float range = 0.0;
     // parse config
     std::string token;
     while (is >> std::ws, !is.eof()) {
       ReadToken(is, false, &token);
       /**/ if (token == "<LearnRateCoef>") ReadBasicType(is, false, &learn_rate_coef);
       else if (token == "<LOrder>") ReadBasicType(is, false, &l_order);
       else if (token == "<LStride>") ReadBasicType(is, false, &l_stride);
       else KALDI_ERR << "Unknown token " << token << ", a typo in config?"
         << " (LearnRateCoef|LOrder|LStride)";
     }

     //init
     learn_rate_coef_ = learn_rate_coef;
     l_order_ = l_order;
     l_stride_ = l_stride;

     // initialize filter
     range = sqrt(6)/sqrt(l_order + input_dim_);
     l_filter_.Resize(l_order, input_dim_, kSetZero);
     RandUniform(0.0, range, &l_filter_);
   }

   void ReadData(std::istream &is, bool binary) {
     // optional learning-rate coefs
     if ('<' == Peek(is, binary)) {
       ExpectToken(is, binary, "<LearnRateCoef>");
       ReadBasicType(is, binary, &learn_rate_coef_);
     }
     if ('<' == Peek(is, binary)) {
       ExpectToken(is, binary, "<LOrder>");
       ReadBasicType(is, binary, &l_order_);
     }
     if ('<' == Peek(is, binary)) {
       ExpectToken(is, binary, "<LStride>");
       ReadBasicType(is, binary, &l_stride_);
     }      
     // weights
     l_filter_.Read(is, binary);
     KALDI_ASSERT(l_filter_.NumRows() == l_order_);
     KALDI_ASSERT(l_filter_.NumCols() == input_dim_);

   }

   void WriteData(std::ostream &os, bool binary) const {
     WriteToken(os, binary, "<LearnRateCoef>");
     WriteBasicType(os, binary, learn_rate_coef_);
     WriteToken(os, binary, "<LOrder>");
     WriteBasicType(os, binary, l_order_);
     WriteToken(os, binary, "<LStride>");
     WriteBasicType(os, binary, l_stride_);
     // weights
     l_filter_.Write(os, binary);
   }

   void ResetMomentum(void)
   {
   }

   int32 NumParams() const { 
     return l_filter_.NumRows()*l_filter_.NumCols(); 
   }

   void GetParams(VectorBase<BaseFloat>* wei_copy) const {
     KALDI_ASSERT(wei_copy->Dim() == NumParams());
     int32 l_filter_num_elem = l_filter_.NumRows() * l_filter_.NumCols();
     wei_copy->Range(0, l_filter_num_elem).CopyRowsFromMat(Matrix<BaseFloat>(l_filter_));
   }

   void SetParams(const VectorBase<BaseFloat> &wei_copy) {
     KALDI_ASSERT(wei_copy.Dim() == NumParams());
     int32 l_filter_num_elem = l_filter_.NumRows() * l_filter_.NumCols();
     l_filter_.CopyRowsFromVec(wei_copy.Range(0, l_filter_num_elem));
   }

   void GetGradient(VectorBase<BaseFloat>* wei_copy) const {
     KALDI_ASSERT(wei_copy->Dim() == NumParams());
   }

   std::string Info() const {
     return std::string("\n  l_filter") + MomentStatistics(l_filter_);
   }
   std::string InfoGradient() const {
     return std::string("\n, lr-coef ") + ToString(learn_rate_coef_) +
       ", l_order " + ToString(l_order_) +
       ", l_stride " + ToString(l_stride_);
   }

   void PropagateFnc(const CuMatrixBase<BaseFloat> &in, CuMatrixBase<BaseFloat> *out) {

     out->GenUniMemory(in, l_filter_, flags_, l_order_, l_stride_);
   }

   void BackpropagateFnc(const CuMatrixBase<BaseFloat> &in, const CuMatrixBase<BaseFloat> &out,
     const CuMatrixBase<BaseFloat> &out_diff, CuMatrixBase<BaseFloat> *in_diff) {
   
     const BaseFloat lr = opts_.learn_rate * learn_rate_coef_;

     in_diff->UniMemoryErrBack(out_diff, l_filter_,  flags_, l_order_, l_stride_);

     l_filter_.GetLfilterErr(out_diff, in, flags_, l_order_, l_stride_, lr);
   }

   void Update(const CuMatrixBase<BaseFloat> &input, const CuMatrixBase<BaseFloat> &diff) {
     ///const BaseFloat lr = opts_.learn_rate * learn_rate_coef_;
   }


 private:
   CuMatrix<BaseFloat> l_filter_;
   CuVector<BaseFloat> flags_;

   BaseFloat learn_rate_coef_;
   int l_order_;
   int l_stride_;
 };

} // namespace nnet1
} // namespace kaldi

#endif
