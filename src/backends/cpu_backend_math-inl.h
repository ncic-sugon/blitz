#ifndef SRC_BACKENDS_CPU_BACKEND_COMMON_INL_H_
#define SRC_BACKENDS_CPU_BACKEND_COMMON_INL_H_

static void RectlinApplyFunc(
  const CPUTensor<DType>* input, CPUTensor<DType>* output,
  DType slope) {
  CHECK_EQ(input->size(), output->size());
  #pragma omp parallel for
  for (size_t i = 0; i < input->size(); ++i) {
    (*output)[i] = std::max((*input)[i], static_cast<DType>(0)) +
      slope * std::min((*input)[i], static_cast<DType>(0));
  }
}

static void RectlinDerivativeFunc(
  const CPUTensor<DType>* input, CPUTensor<DType>* output,
  DType slope) {
  CHECK_EQ(input->size(), output->size());
  #pragma omp parallel for
  for (size_t i = 0; i < input->size(); ++i) {
    DType greater = (*input)[i] > 0 ? 1 : 0;
    DType less = (*input)[i] <= 0 ? slope : 0;
    (*output)[i] = (greater + less) * (*output)[i];
  }
}

static void LogisticApplyFunc(
  const CPUTensor<DType>* input, CPUTensor<DType>* output) {
  CHECK_EQ(input->size(), output->size());
  #pragma omp parallel for
  for (size_t i = 0; i < input->size(); ++i) {
    (*output)[i] = 1 / (exp(-(*input)[i]) + 1);
  }
}

static void LogisticDerivativeFunc(
  const CPUTensor<DType>* input, CPUTensor<DType>* output) {
  CHECK_EQ(input->size(), output->size());
  // TODO(keren) not short cut version
}

static void SoftmaxApplyFunc(
  const CPUTensor<DType>* input, CPUTensor<DType>* output) {
  CHECK_EQ(input->size(), output->size());
  size_t batch_size = input->shape()[0];
  size_t dim = input->size() / batch_size;
  #pragma omp parallel for
  for (size_t i = 0; i < batch_size; ++i) {
    DType sum = 0;
    for (size_t j = 0; j < dim; ++j) {
      size_t index = i * dim + j;
      (*output)[index] = exp((*input)[index]);
      sum += (*output)[index];
    }
    for (size_t j = 0; j < dim; ++j) {
      (*output)[i * dim + j] /= sum;
    }
  }
}

static void SoftmaxDerivativeFunc(
  const CPUTensor<DType>* input, CPUTensor<DType>* output) {
  CHECK_EQ(input->size(), output->size());
  // TODO(keren) not short cut version
}

static DType CrossEntropyBinaryApplyFunc(
  const CPUTensor<DType>* input, const CPUTensor<DType>* target) {
  CHECK_EQ(input->size(), target->size());
  DType private_output = 0;
  DType output = 0;
  #pragma omp parallel firstprivate(private_output)
  {
    #pragma omp for
    for (size_t i = 0; i < input->size(); ++i) {
      private_output += -utils::CPUSafeLog((*input)[i]) * (*target)[i] -
      utils::CPUSafeLog(1 - (*input)[i]) * (1 - (*target)[i]);
    }
    #pragma omp atomic
    output += private_output;
  }
  output /= (input->shape())[0];
  return output;
}

static DType SquareMeanApplyFunc(
  const CPUTensor<DType>* input, const CPUTensor<DType>* target) {
  CHECK_EQ(input->size(), target->size());
  size_t batch_size = input->shape()[0];
  size_t dim = input->size() / batch_size;
  DType sum_square = 0;
  DType sum = 0;
  #pragma omp parallel firstprivate(sum_square)
  {
    #pragma omp for
    for (size_t i = 0; i < batch_size; ++i) {
      for (size_t j = 0; j < dim; ++j) {
        sum_square += pow((*input)[i * dim + j] -
          (*target)[i * dim + j], 2);
      }
    }
    #pragma omp atomic
    sum += sum_square;
  }
  return sum / (2 * batch_size);
}

static void SquareMeanDerivativeFunc(
  const CPUTensor<DType>* input, const CPUTensor<DType>* target,
  CPUTensor<DType>* output) {
  CHECK_EQ(input->size(), target->size());
  CHECK_EQ(output->size(), target->size());
  MinusFunc(input, target, output);
}

static DType AbsMeanApplyFunc(
  const CPUTensor<DType>* input, const CPUTensor<DType>* target) {
  CHECK_EQ(input->size(), target->size());
  size_t batch_size = input->shape()[0];
  size_t dim = input->size() / batch_size;
  DType sum_abs = 0;
  DType sum = 0;
  #pragma omp parallel firstprivate(sum_abs)
  {
    #pragma omp for
    for (size_t i = 0; i < batch_size; ++i) {
      for (size_t j = 0; j < dim; ++j) {
        sum_abs += fabs((*input)[i * dim + j] -
          (*target)[i * dim + j]);
      }
    }
    #pragma omp atomic
    sum += sum_abs;
  }
  return sum / batch_size;
}

static void AbsMeanDerivativeFunc(
  const CPUTensor<DType>* input, const CPUTensor<DType>* target,
  CPUTensor<DType>* output) {
  CHECK_EQ(input->size(), target->size());
  CHECK_EQ(output->size(), target->size());
  size_t batch_size = input->shape()[0];
  size_t dim = input->size() / batch_size;
  #pragma omp parallel for
  for (size_t i = 0; i < batch_size; ++i) {
    for (size_t j = 0; j < dim; ++j) {
      if ((*input)[i * dim + j] > (*target)[i * dim + j]) {
        (*output)[i * dim + j] = 1;
      } else if ((*input)[i * dim + j] < (*target)[i * dim + j]) {
        (*output)[i * dim + j] = -1;
      } else {
        (*output)[i * dim + j] = 0;
      }
    }
  }
}

static void CrossEntropyBinaryDerivativeFunc(
  const CPUTensor<DType>* input, const CPUTensor<DType>* target,
  CPUTensor<DType>* output) {
  MinusFunc(input, target, output);
}

static DType CrossEntropyMultiApplyFunc(
  const CPUTensor<DType>* input, const CPUTensor<DType>* target) {
  CHECK_EQ(input->size(), target->size());
  DType private_output = 0;
  DType output = 0;
  #pragma omp parallel firstprivate(private_output)
  {
    #pragma omp for
    for (size_t i = 0; i < input->size(); ++i) {
      private_output += utils::CPUSafeLog((*input)[i]) * (*target)[i];
    }
    #pragma omp atomic
    output += -private_output;
  }
  output /= (input->shape())[0];
  return output;
}

static void CrossEntropyMultiDerivativeFunc(
  const CPUTensor<DType>* input, const CPUTensor<DType>* target,
  CPUTensor<DType>* output) {
  MinusFunc(input, target, output);
}

static void BiasForwardFunc(
  const CPUTensor<DType>* input, const CPUTensor<DType>* bias,
  CPUTensor<DType>* output) {
  CHECK_EQ(input->size(), output->size());
  size_t batch_size = input->shape()[0];
  size_t dim = input->size() / batch_size;
  #pragma omp parallel for
  for (size_t i = 0; i < batch_size; ++i) {
    for (size_t j = 0; j < dim; ++j) {
      (*output)[i * dim + j] = (*input)[i * dim + j] + (*bias)[j];
    }
  }
}

static void BiasBackwardUpdateFunc(
  const CPUTensor<DType>* input, CPUTensor<DType>* update) {
  size_t batch_size = input->shape()[0];
  size_t dim = input->size() / batch_size;
  #pragma omp parallel for
  for (size_t i = 0; i < dim; ++i) {
    for (size_t j = 0; j < batch_size; ++j) {
      (*update)[i] += (*input)[j * dim + i];
    }
  }
}

static void BatchNormForwardFunc(
  const CPUTensor<DType>* input,
  const CPUTensor<DType>* gamma,
  const CPUTensor<DType>* beta,
  CPUTensor<DType>* input_var,
  CPUTensor<DType>* input_hat,
  CPUTensor<DType>* output,
  DType epsilon) {
  size_t batch_size = input->shape()[0];
  size_t dim = input->size() / batch_size;
  input_var->Fill(0);
  input_hat->Fill(0);
  #pragma omp parallel for
  for (size_t i = 0; i < dim; ++i) {
    DType mean = 0.0;
    for (size_t j = 0; j < batch_size; ++j) {
      mean += (*input)[j * dim + i];
    }
    mean /= batch_size;

    DType var;
    for (size_t j = 0; j < batch_size; ++j) {
      var = (*input)[j * dim + i] - mean;
      (*input_var)[i] += var * var;
    }
    (*input_var)[i] /= batch_size;

    DType divider = sqrt((*input_var)[i] + epsilon);

    size_t index = 0;
    for (size_t j = 0; j < batch_size; ++j) {
      index = j * dim + i;
      (*input_hat)[index] = ((*input)[index] -
        mean) / divider;
      (*output)[index] = (*gamma)[i] * (*input_hat)[index] +
        (*beta)[i];
    }
  }
}

static void BatchNormBackwardFunc(
  const CPUTensor<DType>* backward_input,
  const CPUTensor<DType>* forward_input_hat,
  const CPUTensor<DType>* forward_input_var,
  const CPUTensor<DType>* gamma,
  CPUTensor<DType>* gamma_update,
  CPUTensor<DType>* beta_update,
  CPUTensor<DType>* output,
  DType epsilon) {
  size_t batch_size = backward_input->shape()[0];
  size_t dim = backward_input->size() / batch_size;
  #pragma omp parallel for
  for (size_t i = 0; i < dim; ++i) {
    for (size_t j = 0; j < batch_size; ++j) {
      const size_t index = j * dim + i;
      (*gamma_update)[i] += (*forward_input_hat)[index] *
        (*backward_input)[index];
      (*beta_update)[i] += (*backward_input)[index];
    }

    DType xhat;
    for (size_t j = 0; j < batch_size; ++j) {
      const size_t index = j * dim + i;
      xhat = ((*forward_input_hat)[index] * (*gamma_update)[i] +
        (*beta_update)[i]) / batch_size;
      (*output)[index] = (*gamma)[i] * ((*backward_input)[index] -
        xhat) / sqrt((*forward_input_var)[i] + epsilon);
    }
  }
}

static void GradientdescentFunc(
  CPUTensor<DType>* weight,
  CPUTensor<DType>* gradient,
  CPUTensor<DType>* velocity,
  DType momentum_coef,
  DType learning_rate,
  DType decay,
  size_t batch_size) {
  CHECK_EQ(weight->size(), gradient->size());
  CHECK_EQ(gradient->size(), velocity->size());
#ifdef BLITZ_DEVELOP
  LOG(INFO) << "weight size: " << weight->size();
  LOG(INFO) << "momentum_coef: " << momentum_coef;
  LOG(INFO) << "learning_rate: " << learning_rate;
  LOG(INFO) << "decay: " << decay;
  LOG(INFO) << "batch_size: " << batch_size;
#endif
  #pragma omp parallel for
  for (size_t i = 0; i < velocity->size(); ++i) {
    (*gradient)[i] /= batch_size;
    (*velocity)[i] = (*velocity)[i] * momentum_coef - learning_rate *
      ((*gradient)[i] + decay * (*weight)[i]);
    (*weight)[i] = (*weight)[i] + (*velocity)[i];
  }
}

static void MatrixMultiplyFunc(
  const CPUTensor<DType>* left,
  const CPUTensor<DType>* right,
  CPUTensor<DType>* output, 
  bool transa,
  bool transb,
  DType alpha,
  DType beta,
  BLITZ_ALGORITHM algorithm) {
  size_t dim_left = transa ? left->size() / (left->shape())[0] :
    (left->shape())[0];
  size_t dim_right = transb ? (right->shape())[0] :
    right->size() / (right->shape())[0];
  size_t dim_common_left = transa ? (left->shape())[0] :
    left->size() / (left->shape())[0];
  size_t dim_common_right = transb ? right->size() / (right->shape())[0] :
    (right->shape())[0];
  CHECK_EQ(dim_common_left, dim_common_right);
  utils::Gemm<CPUTensor, DType>(
    const_cast<CPUTensor<DType>*>(left)->data(),
    const_cast<CPUTensor<DType>*>(right)->data(),
    output->data(), 
    transa, transb, 
    alpha, beta,
    dim_left, dim_right, dim_common_left);
}

static void Transpose2DFunc(
  const CPUTensor<DType>* input, CPUTensor<DType>* output) {
  size_t dim_left = input->shape()[0];
  size_t dim_right = input->shape()[1];
  CHECK_EQ(dim_left, output->shape()[1]);
  CHECK_EQ(dim_right, output->shape()[0]);
  #pragma omp parallel for collapse(2)
  for (size_t i = 0; i < dim_left; ++i) {
    for (size_t j = 0; j < dim_right; ++j) {
      (*output)[j * dim_left + i] = (*input)[i * dim_right + j];
    }
  }
}

static void MaximumFunc(
  const CPUTensor<DType>* left, const CPUTensor<DType>* right,
  CPUTensor<DType>* output) {
  CHECK_EQ(left->size(), right->size());
  CHECK_EQ(right->size(), output->size());
  #pragma omp parallel for
  for (size_t i = 0; i < left->size(); ++i) {
    (*output)[i] = std::max((*left)[i], (*right)[i]);
  }
}

static void MinusFunc(
  const CPUTensor<DType>* left, const CPUTensor<DType>* right,
  CPUTensor<DType>* output) {
  CHECK_EQ(left->size(), right->size());
  CHECK_EQ(right->size(), output->size());
  #pragma omp parallel for
  for (size_t i = 0; i < left->size(); ++i) {
    (*output)[i] = (*left)[i] - (*right)[i];
  }
}

static DType SumFunc(
  const CPUTensor<DType>* input) {
  DType output = 0;
  for (size_t i = 0; i < input->size(); ++i) {
    output += (*input)[i];
  }
  return output;
}

static void AddFunc(
  const CPUTensor<DType>* left, const CPUTensor<DType>* right,
  CPUTensor<DType>* output) {
  CHECK_EQ(left->size(), right->size());
  CHECK_EQ(right->size(), output->size());
  #pragma omp parallel for
  for (size_t i = 0; i < left->size(); ++i) {
    (*output)[i] = (*left)[i] + (*right)[i];
  }
}

static void MultiplyFunc(
  const CPUTensor<DType>* left, const CPUTensor<DType>* right,
  CPUTensor<DType>* output) {
  CHECK_EQ(left->size(), right->size());
  CHECK_EQ(right->size(), output->size());
  #pragma omp parallel for
  for (size_t i = 0; i < left->size(); ++i) {
    (*output)[i] = (*left)[i] * (*right)[i];
  }
}


static void MultiplyFunc(
  const CPUTensor<DType>* left, CPUTensor<DType>* output,
  DType right) {
  CHECK_EQ(left->size(), output->size());
  #pragma omp parallel for
  for (size_t i = 0; i < left->size(); ++i) {
    (*output)[i] = (*left)[i] * right;
  }
}

static void MakeBinaryMaskFunc(
  CPUTensor<DType>* output,
  DType low,
  DType high,
  DType keep) { 
  UniformDistributionFunc(output, low, high);
  #pragma omp parallel for
  for (size_t i = 0; i < output->size(); ++i) {
    if ((*output)[i] < keep) {
      (*output)[i] = DType(1);
    } else {
      (*output)[i] = DType(0);
    }
  }
}

static void ConstantDistributionFunc(
  CPUTensor<DType>* output, DType val) {
  output->Fill(val);
}

static void NormalDistributionFunc(
  CPUTensor<DType>* output, DType loc, DType scale) {
  // TODO(keren) synchronized seed
  static unsigned int seed = 0;
  boost::mt19937 rng((++seed) + time(NULL));
  boost::normal_distribution<DType> nd(loc, scale);  // convert to DType
  boost::variate_generator<boost::mt19937&,
    boost::normal_distribution<DType> > var_nor(rng, nd);
  var_nor.distribution().reset();
  for (size_t i = 0; i < output->size(); ++i) {
    (*output)[i] = var_nor();
  }
}

static void UniformDistributionFunc(
  CPUTensor<DType>* output, DType low, DType high) {
  // TODO(keren) synchronized seed
  static unsigned int seed = 0;
  boost::mt19937 rng((++seed) + time(NULL));
  boost::uniform_real<DType> ur(low, high);  // convert to DType
  boost::variate_generator<boost::mt19937&,
    boost::uniform_real<DType> > var_uni(rng, ur);
  var_uni.distribution().reset();
  for (size_t i = 0; i < output->size(); ++i) {
    (*output)[i] = var_uni();
  }
}

static float EvaluateClassifyFunc(
  const CPUTensor<DType>* output, const CPUTensor<DType>* target) {
  size_t batch_size = output->shape()[0];
  size_t dim = output->size() / batch_size;
  float correct = 0.0f;
  #pragma omp parallel for
  for (size_t i = 0; i < batch_size; ++i) {
    DType max = (*output)[i * dim];
    size_t max_index = 0;
    for (size_t j = 1; j < dim; ++j) {
      if (max < (*output)[i * dim + j]) {
        max_index = j;
        max = (*output)[i * dim + j];
      }
    }
    if ((*target)[i * dim + max_index] == static_cast<DType>(1)) {
      #pragma omp atomic
      correct += 1.0f;
    }
  }

  return correct / batch_size;
}

static float EvaluateRegressFunc(
  const CPUTensor<DType>* output, const CPUTensor<DType>* target) {
  size_t batch_size = output->shape()[0];
  size_t dim = output->size() / batch_size;
  float result = 0.0;
  float correct = 0.0;
  #pragma omp parallel for firstprivate(correct)
  for (size_t i = 0; i < batch_size; ++i) {
    for (size_t j = 0; j < dim; ++j) {
      correct += fabs((*output)[i] - (*target)[i]);
    }
    #pragma omp atomic
    result += correct;
  }
  return result / batch_size;
}

#endif  // SRC_BACKENDS_CPU_BACKEND_COMMON_INL_H_
