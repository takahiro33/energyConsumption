
suppressPackageStartupMessages(library (ggplot2))
suppressPackageStartupMessages(library (scales))
suppressPackageStartupMessages(library (optparse))
suppressPackageStartupMessages(library (doBy))

# set some reasonable defaults for the options that are needed
#option_list <- list (
# make_option(c("-f", "--file"), type="character", default="results/fake/5/rate-trace",
#           help="File which holds the raw rate data.\n\t\t[Default \"%default\"]"),)
#make_option(c("-o", "--output"), type="character", default=".",
#            help="Output directory for graphs.\n\t\t[Default \"%default\"]"),
#make_option(c("-t", "--title"), type="character", default="Scenario",
#           help="Title for the graph")


#opt = parse_args(OptionParser(option_list=option_list, description="Creates graphs from ndnSIM L3 Data rate Tracer data"))

dir <- "/results/"
        ndn/speed_10/MN_2
speed = c(0, 2, 4, 6, 8)
mode = c("nnn", "ndn")
mobileNodes = c(1,2,5)
output <- matrix(1:120, nrow = 30, ncol = 4)
count <- 1

    # Load the parser 
for(mn in mobileNodes){
  for(s in speed){
    for(m in mode){
      
    fileName = paste(dir,m,"/speed_",s,"/MN_",mn,"/rate-trace",sep="")
    data = read.table(fileName, header = T)

    # Filter for a mobile node 
    data <- data[data$Node < mn,]
    data <- data[data$FaceId == 0,]
    data <- data[data$Type == "InData",]
    data$Kilobits <- data$Kilobytes * 8
    endTime <- max(data[,"Time"])
    averageDataRate <- sum(data[,"Kilobits"]) / endTime / (mn+1)
    output[count,] <- c(mn,s,m,averageDataRate)
    count <- count + 1
    }
  }
}

colnames(output) <- c("Number of MN","Speed","Mode", "DataRate") 


    # Draw the graphs 
for(mn in mobileNodes){

  intname = paste("MN Data rate (", mn, "mobile node(s))")
  plotData2 = as.data.frame(output)
  plotData = subset (plotData2, mobileNodes == mn)
  plotData$Speed = as.numeric(as.character(plotData$Speed))
  plotData$DataRate = as.numeric(as.character(plotData$DataRate))
  plotData$Mode = factor(plotData$Mode)

  g.int <- ggplot (plotData, aes(color=Mode, group=Mode) ) + 
    geom_line(aes(x=Speed, y=DataRate, linetype=Mode), size=1.5, lineType="dashed", colour = "black") +
    geom_point(aes(x=Speed, y=DataRate, shape=Mode), size=5, colour = "black") +
    # Font settings
    theme_bw()+
    theme(title = element_text(size=35), 
          axis.line = element_line(colour = 'black'),
          axis.title.x = element_text(size=30), axis.title.y = element_text(size=30),
          axis.text.x = element_text(size=24),axis.text.y = element_text(size=24),
          legend.title = element_text(size=26), legend.text = element_text(size=26)
          )+
    ggtitle (intname) +
    ylab ("Rate [Kbits/s]") +
    xlab ("Mobile Node's speed [Km/h]") 

  # make png file
outInpng = sprintf("%sgraphs/MN_%dgraph.png", dir, mn)
png (outInpng, width=1024, height=768)
print (g.int)
x = dev.off ()
}

