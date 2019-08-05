// [[Rcpp::depends(RcppArmadillo)]]
#include <RcppArmadillo.h>
#include <functional>
#include <unordered_map>
#include <string>
using namespace Rcpp;
using namespace arma;
using namespace std;

// check if prob is probability or not
// [[Rcpp::export(.isProbability)]]
bool isProb(double prob) {
  return (prob >= 0 && prob <= 1);
}

// doubt
// [[Rcpp::export(.isGenRcpp)]]
bool isGen(NumericMatrix gen) {
  for (int i = 0; i < gen.nrow(); i++)
    for (int j = 0; j < gen.ncol(); j++)
      if ((i == j && gen(i, j) > 0) || (i != j && gen(i, j) < 0))  
        return false;

  return true;
}

// Declared in probabilistic.cpp
SEXP commClassesKernel(NumericMatrix P);

// method to convert into canonic form a markovchain object
// [[Rcpp::export(.canonicFormRcpp)]]
SEXP canonicForm(S4 object) {
  NumericMatrix P = object.slot("transitionMatrix");
  bool byrow = object.slot("byrow");
  
  if (!byrow)
    P = transpose(P);
  
  List comclasList = commClassesKernel(P);
  LogicalVector vu = comclasList["closed"];
  NumericVector u, w; 

  for (int i = 0; i < vu.size(); i ++) {
    if(vu[i]) u.push_back(i);
    else w.push_back(i);
  }
  
  LogicalMatrix Cmatr = comclasList["classes"];
  NumericVector R, p;
  LogicalVector crow;
  
  while (u.size() > 0) {
    R.push_back(u[0]);
    crow = Cmatr(u[0], _);

    for (int i = 0; i < crow.size(); i++) 
      vu[i] = vu[i] * !crow[i];

    u = NumericVector::create();

    for (int i = 0; i < vu.size(); i ++) 
      if(vu[i]) u.push_back(i);
  }
  
  for (int i = 0; i < R.size(); i ++) {
    crow = Cmatr(R[i], _);

    for (int j = 0; j < crow.size(); j++) 
      if(crow[j]) p.push_back(j);
  }
  
  for (NumericVector::iterator it = w.begin(); it != w.end(); it++)
    p.push_back(*it);
  
  NumericMatrix Q(p.size());
  CharacterVector rnames(P.nrow());
  CharacterVector cnames(P.ncol());
  CharacterVector r = rownames(P);
  CharacterVector c = colnames(P);

  for (int i = 0; i < p.size(); i ++) {
    rnames[i] = r[p[i]];
    for (int j = 0; j < p.size(); j ++) {
      Q(i, j) = P(p[i], p[j]);
      cnames[j] = c[p[j]];
    }
  }
  
  if (!byrow)
    Q = transpose(Q);
  
  S4 out("markovchain"); 
  Q.attr("dimnames") = List::create(rnames, cnames);
  out.slot("transitionMatrix") = Q;
  out.slot("name") = object.slot("name");
  
  return out;
}




// Function to sort a matrix of vectors lexicographically
NumericMatrix lexicographicalSort(NumericMatrix m) {
  int numCols = m.ncol();
  int numRows = m.nrow();
  
  if (numRows > 0 && numCols > 0) {
    vector<vector<double>> x(numRows, vector<double>(numCols));

    for (int i = 0; i < numRows; ++i)
      for (int j = 0; j < numCols; ++j)
        x[i][j] = m(i,j);
    
    sort(x.begin(), x.end());
    
    NumericMatrix result(numRows, numCols);
    
    for (int i = 0; i < numRows; ++i)
      for (int j = 0; j < numCols; ++j)
        result(i, j) = x[i][j];
    
    colnames(result) = colnames(m);
    return result;
  } else {
    return m;
  }
}

inline bool approxEqual(const double& a, const double& b) {
  if (a >= b)
    return (a - b) <= 1E-7;
  else
    return approxEqual(b, a);
}

inline bool approxEqual(const cx_double& a, const cx_double& b){
  double x = a.real() - b.real();
  double y = a.imag() - b.imag();
  
  return (x*x - y*y) <= 1E-14;
}

mat computeSteadyStates(NumericMatrix t, bool byrow) {
  if (byrow)
    t = transpose(t);
  
  cx_mat transitionMatrix = as<cx_mat>(t);
  cx_vec eigvals;
  cx_mat eigvecs;
  // 1 + 0i
  cx_double cx_one(1.0, 0);
  
  // If transition matrix is hermitian (symmetric in real case), use
  // more efficient implementation to get the eigenvalues and vectors
  if (transitionMatrix.is_hermitian()) {
    vec real_eigvals;
    eig_sym(real_eigvals, eigvecs, transitionMatrix);
    eigvals.resize(real_eigvals.size());
    
    // eigen values are real, but we need to cast them to complex values
    // to perform the rest of the algorithm
    for (int i = 0; i < real_eigvals.size(); ++i)
      eigvals[i] = cx_double(real_eigvals[i], 0);
  } else {
    eig_gen(eigvals, eigvecs, transitionMatrix, "balance");
  }

  std::vector<int> whichOnes;
  std::vector<double> colSums;
  double colSum;
  mat real_eigvecs = real(eigvecs);
  int numRows = real_eigvecs.n_rows;
  
  // Search for the eigenvalues which are 1 and store 
  // the sum of the corresponding eigenvector
  for (int j = 0; j < eigvals.size(); ++j) {
    if (approxEqual(eigvals[j], cx_one)) {
      whichOnes.push_back(j);
      colSum = 0;
      
      for (int i = 0; i < numRows; ++i)
        colSum += real_eigvecs(i, j);
      
      colSums.push_back((colSum != 0 ? colSum : 1));
    }
  }

  // Normalize eigen vectors
  int numCols = whichOnes.size();
  mat result(numRows, numCols);
  
  for (int j = 0; j < numCols; ++j)
    for (int i = 0; i < numRows; ++i)
        result(i, j) = real_eigvecs(i, whichOnes[j]) / colSums[j];
  
  if (byrow)
    result = result.t();
  
  return result;
}

bool anyElement(mat matrix, bool (*condition)(const double&)) {
  int numRows = matrix.n_rows;
  int numCols = matrix.n_cols;
  bool found = false;
  
  for (int i = 0; i < numRows && !found; ++i)
    for (int j = 0; j < numCols && !found; ++j)
      found = condition(matrix(i, j));
  
  return found;
}

// Defined in probabilistic.cpp
List recurrentClasses(S4 object);

// Precondition: the matrix should be stochastic by rows
NumericMatrix steadyStatesByRecurrentClasses(S4 object) {
  List recClasses = recurrentClasses(object);
  int numRecClasses = recClasses.size();
  NumericMatrix transitionMatrix = object.slot("transitionMatrix");
  
  CharacterVector states = object.slot("states");
  int numCols = transitionMatrix.ncol();
  NumericMatrix steady(numRecClasses, numCols);
  unordered_map<string, int> stateToIndex;
  int steadyStateIndex = 0;
  
  // Map each state to the index it has
  for (int i = 0; i < states.size(); ++i) {
    string state = (string) states[i];
    stateToIndex[state] = i;
  }
  
  // For each recurrent class, there must be an steady state
  for (CharacterVector recurrentClass : recClasses) {
    int recClassSize = recurrentClass.size();
    NumericMatrix subMatrix(recClassSize, recClassSize);
    
    // Fill the submatrix corresponding to the current steady class
    // Note that for that we have to subset the matrix with the indices
    // the states in the recurrent class ocuppied in the transition matrix
    for (int i = 0; i < recClassSize; ++i) {
      int r = stateToIndex[(string) recurrentClass[i]];
      
      for (int j = 0; j < recClassSize; ++j) {
        int c = stateToIndex[(string) recurrentClass[j]];
        subMatrix(i, j) = transitionMatrix(r, c);
      }
    }
    
    // Compute the steady states for the given submatrix
    mat steadySubMatrix = computeSteadyStates(subMatrix, true);
    
    // There should only be one steady state for that matrix
    // Make sure of it
    if (steadySubMatrix.n_rows != 1)
      stop("Could not compute steady states with recurrent classes method");
    
    for (int i = 0; i < recClassSize; ++i) {
      int c = stateToIndex[(string) recurrentClass[i]];
      steady(steadyStateIndex, c) = steadySubMatrix(0, i);
    }
    
    ++steadyStateIndex;
  }
    
  colnames(steady) = states;
  
  return steady;
}


// [[Rcpp::export(.steadyStatesRcpp)]]
NumericMatrix steadyStates(S4 obj) {
  NumericMatrix transitions = obj.slot("transitionMatrix");
  CharacterVector states = obj.slot("states");
  bool byrow = obj.slot("byrow");
  S4 object("markovchain");

  if (!byrow) {
    object.slot("transitionMatrix") = transpose(transitions);
    object.slot("states") = states;
    object.slot("byrow") = true;
    transitions = transpose(transitions);
  } else {
    object = obj;
  }
  
  // Try to compute the steady states computing the eigenvectors
  // associated with the eigenvalue 1, else compute steady states
  // using recurrent classes (there is one steady state associated
  // with each recurrent class)
  NumericMatrix result;
  mat steady;
  steady = computeSteadyStates(transitions, true);
  auto isNegative = [](const double& x) { return x < 0; };
  
  if (anyElement(steady, isNegative)) {
    result = steadyStatesByRecurrentClasses(object);
  } else {
    result = wrap(steady);
    colnames(result) = colnames(transitions);
  }

  result = lexicographicalSort(result);
  
  if (!byrow)
      result = transpose(result);

  return result;
}
