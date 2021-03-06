% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/data.R
\docType{data}
\name{sales}
\alias{sales}
\title{Sales Demand Sequences}
\format{
An object of class \code{matrix} (inherits from \code{array}) with 269 rows and 5 columns.
}
\usage{
data("sales")
}
\description{
Sales demand sequences of five products (A, B, C, D, E).
             Each row corresponds to a sequence. First row corresponds to Sequence A, 
             Second row to Sequence B and so on.
}
\details{
The example can be used to fit High order multivariate
         markov chain.
}
\examples{
data("sales")
# fitHighOrderMultivarMC(seqMat = sales, order = 2, Norm = 2)

}
\keyword{datasets}
