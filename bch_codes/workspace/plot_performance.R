library(readr)
results <- read_csv("workspace/results.csv")

matplot(results[,1], cbind(results[,2], results[,3]), type = "l", lty = 1, log="y", 
  col = c("red", "green"), xlab = "Bit flip probability", 
  ylab = "Number of errors", main = "Performance of BCH(31, 16, 3)")

legend("bottomright", legend = c("None", "BCH(31, 16, 3)"), 
  col = c("red", "green"), 
  lty = 1)
