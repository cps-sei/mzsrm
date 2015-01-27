/*
Copyright (c) 2014 Carnegie Mellon University.
All Rights Reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following acknowledgments and disclaimers.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following acknowledgments and disclaimers in the documentation and/or other materials provided with the distribution.
3. Products derived from this software may not include �Carnegie Mellon University,� "SEI� and/or �Software Engineering Institute" in the name of such derived product, nor shall �Carnegie Mellon University,� "SEI� and/or �Software Engineering Institute" be used to endorse or promote products derived from this software without prior written permission. For written permission, please contact permission@sei.cmu.edu.
ACKNOWLEDMENTS AND DISCLAIMERS:
Copyright 2014 Carnegie Mellon University

This material is based upon work funded and supported by the Department of Defense under Contract No. FA8721-05-C-0003 with Carnegie Mellon University for the operation of the Software Engineering Institute, a federally funded research and development center.

Any opinions, findings and conclusions or recommendations expressed in this material are those of the author(s) and do not necessarily reflect the views of the United States Department of Defense.

NO WARRANTY. THIS CARNEGIE MELLON UNIVERSITY AND SOFTWARE ENGINEERING INSTITUTE MATERIAL IS FURNISHED ON AN �AS-IS� BASIS. CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER EXPRESSED OR IMPLIED, AS TO ANY MATTER INCLUDING, BUT NOT LIMITED TO, WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY, OR RESULTS OBTAINED FROM USE OF THE MATERIAL. CARNEGIE MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.

This material has been approved for public release and unlimited distribution.

Carnegie Mellon� is registered in the U.S. Patent and Trademark Office by Carnegie Mellon University.

DM-0000890

 */

package edu.cmu.sei.ZeroSlackRM;
import java.io.FileWriter;
import java.util.HashMap;
import java.util.TreeSet;
import java.util.Vector;


public class ZSRMScheduler {
  TreeSet<Task> tasksByCriticality;
  TreeSet<Task> tasksByPriority;
	
	
  protected HashMap<Task, DualExecutionZone> executionZones;
	
  HashMap<Task,String> taskColor = new HashMap<Task,String>();
  String [] colors = new String[]{"red","blue","green","white","black","gray"};
  int colorIndex=0;
	
  public ZSRMScheduler(){
    tasksByCriticality = new TreeSet<Task>(new CriticalityComparator());
    tasksByPriority = new TreeSet<Task>(new PriorityComparator());
    executionZones = new HashMap<Task, DualExecutionZone>();		
  }
	
  public TreeSet<Task> getTasksByPriority(){
    return tasksByPriority;
  }
	
  public void addTask(Task t){
    if (t != null){
      tasksByCriticality.add(t);
      tasksByPriority.add(t);
      executionZones.put(t, new DualExecutionZone(new Vector<SlackRegion>(), new Vector<SlackRegion>(),new Vector<SlackRegion>()));
      taskColor.put(t, colors[colorIndex]);
      colorIndex = (colorIndex+1)%colors.length;
    }
  }
	
  public boolean isSchedulable(){
    boolean schedulable = false;
		
    return schedulable;
  }

  public Task getNextInterferingNonZeroTaskArrival(Task task, Iterable<Task> set, long time, long endOfPeriod){
    Task tret=null;
    long previousArrival=endOfPeriod;
    long currentArrival=0;
		
    for (Task t:set){
      if (getInterferingTaskExecution(task,t)<=0)
        continue;
			
      currentArrival = ((long)(Math.ceil(((double)time)/((double)t.T))*((double)t.T)));
      if (currentArrival < previousArrival){
        tret = t;
        previousArrival = currentArrival;
      }
    }
    return tret;
  }

  public Task getNextArrivingTask(Iterable<Task> set, long time, long endOfPeriod){
    Task tret=null;
    long previousArrival=endOfPeriod;
    long currentArrival=0;
		
    for (Task t:set){
      currentArrival = ((long)(Math.ceil(((double)time)/((double)t.T))*((double)t.T)));
      if (currentArrival < previousArrival){
        tret = t;
        previousArrival = currentArrival;
      }
    }
    return tret;
  }
	
  public long getInterferingTaskExecution(Task task, Task iTask){
    if (iTask == null)
      return 0;
    if (iTask.criticality >= task.criticality){
      if (iTask.RmPriority > task.RmPriority)
        return 0;
      return iTask.Ca;
    } else if (iTask.RmPriority <= task.RmPriority) {
      return iTask.Cs;
    } else if (iTask.RmPriority > task.RmPriority){
      Vector<SlackRegion> v = calculateOptimisticSlack(task, iTask,new RMZoneTaskIterable(tasksByPriority,iTask));
      long localDiscount = getAvailableSlackUpToInstant(iTask.ZeroSlackInstant,v);
      //System.out.println("For Task " + task.T + " and Taskj " + iTask.T + " Local Discount " + localDiscount + " Taskj Crm " + iTask.C_rm);
      if (localDiscount > iTask.Cs)
        localDiscount = iTask.Cs;

      return iTask.Cs - localDiscount;// -- Testing to see if double discount (iTask.Cs > iTask.C_rm ? iTask.Cs - iTask.C_rm : 0); 
    }		
    return 0;
  }

	
  public Vector<SlackRegion> calculateOptimisticSlack(Task bottom, Task task, Iterable<Task> filter){
    Vector <SlackRegion> vector = new Vector<SlackRegion>();
    long previousExecutionRegion=0;
    long currentExecutionRegion=0;
    long virtualExecution=0;
    long nextTaskArrivalTime;
    Task nextArrivingTask;
    long nextSlackTime=0;
		
    do{
      boolean bootstrap=true;
      // next busy period
      do {
        previousExecutionRegion = currentExecutionRegion;
        if (bootstrap){
          // bootstrap next busy period
          Task arriving = getNextArrivingTask(filter,previousExecutionRegion, task.T);
          // FIXME: Is this correct way of handling arriving = null
          if(arriving == null)
            {
              currentExecutionRegion = task.T;
              break;
            }
          previousExecutionRegion += (bottom.criticality > arriving.criticality) ? arriving.Cs : arriving.Ca;
          bootstrap=false;
        }
        currentExecutionRegion=0;
        for (Task oTask:filter){
          long et;
          long localDiscount = 0;
          long interDiscount = 0;
          if(bottom.criticality > oTask.criticality)
            {
              Vector<SlackRegion> v = calculateOptimisticSlack(task, oTask, new RMZoneTaskIterable(tasksByPriority,oTask));
              localDiscount = getAvailableSlackUpToInstant(oTask.ZeroSlackInstant,v);
            }
					
          // FIXME: Should we apply discount here ?
          et = (bottom.criticality > oTask.criticality) ? oTask.Cs : oTask.Ca;
					
          // Applying the local discount
          if(bottom.criticality > oTask.criticality)
            {
              if(et >= localDiscount)
                et -= localDiscount;
              else
                et = 0;
            }
					
          // FIXME: APR - 12: Can we subtract the lower criticality interference over the blocking time
          if(bottom.criticality > oTask.criticality)
            {
              // oTask is a well behaved task - higher criticality
              if(task.RmPriority < oTask.RmPriority)
                {
                  //oTask is a lower priority task
                  //et is the blocking term coming from higher criticality task
                  //the only preemptions over this period are higher priority and higher criticality
                  //subtract from et the floor of lower criticality higher priority interference
                  for (Task iTask:filter){
                    if(iTask.RmPriority < task.RmPriority)
                      {
                        // Picked all the higher priority tasks that interfere with us in critical mode
                        if(iTask.criticality > oTask.criticality && iTask.criticality < task.criticality)
                          {
                            if(bottom.criticality > iTask.criticality)
                              interDiscount += (long)(Math.floor(((double)et) / ((double)iTask.T)) * ((double)iTask.Cs));
                            else
                              interDiscount += (long)(Math.floor(((double)et) / ((double)iTask.T)) * ((double)iTask.Ca));
                          }
                      }
                  }	
                }
            }
					
          if(et >= interDiscount)
            {
              et -= interDiscount;
            }
          else
            {
              et = 0;
              // Should never reach here
            }
					
          currentExecutionRegion += (long)(Math.ceil(((double)previousExecutionRegion) / ((double)oTask.T)) * ((double)et));
					
					
        }
        currentExecutionRegion += virtualExecution;
      } while (previousExecutionRegion < currentExecutionRegion && currentExecutionRegion < task.T);

      // end of busy period.
      nextArrivingTask =getNextArrivingTask(filter,currentExecutionRegion, task.T);
      nextTaskArrivalTime = (nextArrivingTask != null) ? ((long)(Math.ceil(((double)currentExecutionRegion)/((double)nextArrivingTask.T))*((double)nextArrivingTask.T))): task.T;

      if (nextTaskArrivalTime <= task.T)
        nextSlackTime = nextTaskArrivalTime - currentExecutionRegion;
      else 
        nextSlackTime = task.T - currentExecutionRegion;
      if (nextSlackTime >0){
        virtualExecution += nextSlackTime;
        vector.add(new SlackRegion(currentExecutionRegion, nextSlackTime));
        currentExecutionRegion += nextSlackTime;
      }
      //Dio: added "previousExecutionRegion < currentExecutionRegion. Without it 
      //     it gets stuck at times
    } while(previousExecutionRegion < currentExecutionRegion && currentExecutionRegion < task.T);
    return vector;
  }

  public String svgPlotExecutionRegion(Task task, Iterable<Task> filter, long startTime, long endTime){
    String plotString="";
		
    long currentTime = 0;
    //		long taskSwitchingTime=0;
    long taskSwitchingTime=startTime;
    Task eligibleTask=null;
    Task previousEligible = null;
    long currentCriticality=Long.MAX_VALUE;
    long currentRunningRegion=0;
		
    // svg plotting parameters
    long blocksize = 50;
    long fontsize = 12;
    long minZeroSlack = 0;
    long previousMinZeroSlack = 0;
		
    long finishTime = 0;
		
		
    // Calculate total blocking in Critical Mode
    // Accommodate higher priority higher criticality tasks
    do
      {	
        previousMinZeroSlack = minZeroSlack;
        minZeroSlack = 0;
        for (Task taskj:filter){
          if (taskj.criticality<task.criticality ){
            if(taskj.RmPriority>task.RmPriority)
              {
                // Li^hc
                minZeroSlack += getInterferingTaskExecution(task,taskj);
              }
            else
              {
                //Hi^hc
                minZeroSlack += ((long)(Math.ceil((double)previousMinZeroSlack/(double)taskj.T)))*getInterferingTaskExecution(task,taskj);
              }
          }
        } 
      }while(previousMinZeroSlack < minZeroSlack && minZeroSlack <= task.T);
    System.out.println(" For Task " + task.T + " the minZeroSlack is " + minZeroSlack);
    //for (currentTime =0; currentTime < (endTime-startTime); currentTime++){
    for (currentTime =startTime; currentTime < endTime; currentTime++){
      previousEligible = eligibleTask;
      for (Task taskj:filter){
        // Li^hc
        if (taskj.RmPriority>task.RmPriority && taskj.criticality<task.criticality ){
          // FIXME: Check whether the second condition is required
          //if ((currentTime == task.ZeroSlackInstant))	
          if ((currentTime == task.ZeroSlackInstant) || ((task.T-task.ZeroSlackInstant)<minZeroSlack && (currentTime + minZeroSlack)==task.T))
            {
              System.out.println("Releasing "+taskj.T + " for " + getInterferingTaskExecution(task,taskj));
              taskj.remainingComputation = getInterferingTaskExecution(task,taskj);
            }
        } else {
          if ((currentTime %taskj.T) ==0){
            taskj.remainingComputation = getInterferingTaskExecution(task,taskj);
            // try discounting what it already run in the previous execution region (RM region)
            //						taskj.remainingComputation = getInterferingTaskExecution(task,taskj) - taskj.remainingComputation;
          }
        }
      }
			
      eligibleTask = null;
      for (Task taskj:filter){
        // Taskset Li^{hc}
        if (taskj.RmPriority>task.RmPriority && taskj.criticality<task.criticality && taskj.remainingComputation > 0){
          if (eligibleTask == null ){
            if (taskj.remainingComputation >0)
              eligibleTask = taskj;
          } else if (eligibleTask.criticality>taskj.criticality){
            eligibleTask = taskj;
          }
        }
      }
      // Hi^{hc}
      for (Task taskj:filter){
        if (eligibleTask != null){
          currentCriticality = eligibleTask.criticality;
          if (taskj.criticality < currentCriticality && taskj.RmPriority<task.RmPriority && taskj.remainingComputation>0){
            if (taskj.RmPriority < eligibleTask.RmPriority){
              eligibleTask = taskj;
            }
          }
        }
      }
				
      if (eligibleTask == null){
        // Hi^{lc}
        for (Task taskj:filter){
          if (eligibleTask == null){
            if (taskj.remainingComputation>0)
              eligibleTask = taskj;
          } else {
            if (taskj.RmPriority<eligibleTask.RmPriority && taskj.remainingComputation >0){
              eligibleTask=taskj;
            }
          }
        }
      }
			
      if (eligibleTask != null){
        eligibleTask.remainingComputation--;
      }
			
			
      // switch tasks
      if (previousEligible != eligibleTask){

        if (previousEligible!=null){
          // plot from taskSwitchingTime to currentTime

          plotString +="<text xml:space=\"preserve\" "+
            "style=\"font-size:"+fontsize+"px;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Bitstream Vera Sans\""+
            " x=\""+(taskSwitchingTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30 +blocksize+fontsize)+"\">"+(taskSwitchingTime)+"</text>\n";
          //					" x=\""+(taskSwitchingTime+startTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30 +blocksize+fontsize)+"\">"+(taskSwitchingTime+startTime)+"</text>\n";

          //					plotString += "<rect x=\""+(taskSwitchingTime+startTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30)+"\" width=\""+(currentTime-taskSwitchingTime)+"\" height=\""+(blocksize)+"\""+
          plotString += "<rect x=\""+(taskSwitchingTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30)+"\" width=\""+(currentRunningRegion)+"\" height=\""+(blocksize)+"\""+
            " style=\"fill:"+taskColor.get(previousEligible)+";stroke:black;stroke-width:3;"+
            "fill-opacity:1;stroke-opacity:1\"/>\n";


          plotString +="<text xml:space=\"preserve\" "+
            "style=\"font-size:"+fontsize+"px;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Bitstream Vera Sans\""+
            //					" x=\""+(currentTime+startTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30+blocksize+fontsize)+"\">"+(currentTime+startTime)+"</text>\n";
            " x=\""+(currentTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30+blocksize+fontsize)+"\">"+(currentTime)+"</text>\n";
          //				} else if (eligibleTask != null && (currentTime -taskSwitchingTime > 0)){
        } else if (eligibleTask != null && (currentRunningRegion> 0)){
          // I am running but got interrupted
          finishTime = (currentTime);
          if(task.remainingComputation > 0)
            {
              System.out.println("Running for " + currentRunningRegion + " My computation " + task.remainingComputation + " Ending at " + currentTime);
              if(task.remainingComputation >= currentRunningRegion)
                {
                  task.remainingComputation -= currentRunningRegion;
                }
              else
                {
                  finishTime = (currentTime - currentRunningRegion + task.remainingComputation);
                  currentRunningRegion = task.remainingComputation;
                  task.remainingComputation = 0;
                }
						
              plotString +="<text xml:space=\"preserve\" "+
                "style=\"font-size:"+fontsize+"px;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Bitstream Vera Sans\""+
                " x=\""+(taskSwitchingTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30 +blocksize+fontsize)+"\">"+(taskSwitchingTime)+"</text>\n";
              //					" x=\""+(taskSwitchingTime+startTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30 +blocksize+fontsize)+"\">"+(taskSwitchingTime+startTime)+"</text>\n";
	
              //					plotString += "<rect x=\""+(taskSwitchingTime+startTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30)+"\" width=\""+(currentTime-taskSwitchingTime)+"\" height=\""+(blocksize)+"\""+
              plotString += "<rect x=\""+(taskSwitchingTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30)+"\" width=\""+(currentRunningRegion)+"\" height=\""+(blocksize)+"\""+
                " style=\"fill:"+taskColor.get(task)+";stroke:black;stroke-width:3;"+
                "fill-opacity:1;stroke-opacity:1\"/>\n";
	
	
              plotString +="<text xml:space=\"preserve\" "+
                "style=\"font-size:"+fontsize+"px;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Bitstream Vera Sans\""+
                " x=\""+(currentTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30+blocksize+fontsize)+"\">"+(currentTime)+"</text>\n";

              if(finishTime != currentTime)
                plotString +="<text xml:space=\"preserve\" "+
                  "style=\"font-size:"+fontsize+"px;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Bitstream Vera Sans\""+
                  " x=\""+(finishTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30+blocksize+fontsize)+"\">"+(finishTime)+"</text>\n";

              //					" x=\""+(currentTime+startTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30+blocksize+fontsize)+"\">"+(currentTime+startTime)+"</text>\n";
            }
        }
				
        taskSwitchingTime = currentTime;
        currentRunningRegion=0;
      }
      currentRunningRegion++;
    }

    if (previousEligible!=null){
		
      // plot from taskSwitchingTime to currentTime

      plotString +="<text xml:space=\"preserve\" "+
        "style=\"font-size:"+fontsize+"px;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Bitstream Vera Sans\""+
        " x=\""+(taskSwitchingTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30 +blocksize+fontsize)+"\">"+(taskSwitchingTime)+"</text>\n";
      //			" x=\""+(taskSwitchingTime+startTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30 +blocksize+fontsize)+"\">"+(taskSwitchingTime+startTime)+"</text>\n";

      //			plotString += "<rect x=\""+(taskSwitchingTime+startTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30)+"\" width=\""+(currentTime-taskSwitchingTime)+"\" height=\""+(blocksize)+"\""+
      plotString += "<rect x=\""+(taskSwitchingTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30)+"\" width=\""+(currentRunningRegion)+"\" height=\""+(blocksize)+"\""+
        " style=\"fill:"+taskColor.get(previousEligible)+";stroke:black;stroke-width:3;"+
        "fill-opacity:1;stroke-opacity:1\"/>\n";


      plotString +="<text xml:space=\"preserve\" "+
        "style=\"font-size:"+fontsize+"px;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Bitstream Vera Sans\""+
        " x=\""+(currentTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30+blocksize+fontsize)+"\">"+(currentTime)+"</text>\n";
      //			" x=\""+(currentTime+startTime)+"\" y=\""+((previousEligible.RmPriority * blocksize)+ 30+blocksize+fontsize)+"\">"+(currentTime+startTime)+"</text>\n";
    } else if (currentRunningRegion >0){
      // I am running but got interrupted
      if(task.remainingComputation > 0)
        {
          System.out.println("Running for " + currentRunningRegion + " My computation " + task.remainingComputation + " Ending at " + currentTime);

          finishTime = currentTime;
          if(task.remainingComputation >= currentRunningRegion)
            {
              task.remainingComputation -= currentRunningRegion;
            }
          else
            {
              finishTime = (currentTime - currentRunningRegion + task.remainingComputation);
              currentRunningRegion = task.remainingComputation;
              task.remainingComputation = 0;
            }
          System.out.println("Remaining computation for " + task.T + " is " + task.remainingComputation);
          System.out.println("Current Running Region " + currentRunningRegion);
          plotString +="<text xml:space=\"preserve\" "+
            "style=\"font-size:"+fontsize+"px;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Bitstream Vera Sans\""+
            " x=\""+(taskSwitchingTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30 +blocksize+fontsize)+"\">"+(taskSwitchingTime)+"</text>\n";
          //			" x=\""+(taskSwitchingTime+startTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30 +blocksize+fontsize)+"\">"+(taskSwitchingTime+startTime)+"</text>\n";
	
          //			plotString += "<rect x=\""+(taskSwitchingTime+startTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30)+"\" width=\""+(currentTime-taskSwitchingTime)+"\" height=\""+(blocksize)+"\""+
          plotString += "<rect x=\""+(taskSwitchingTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30)+"\" width=\""+(currentRunningRegion)+"\" height=\""+(blocksize)+"\""+
            " style=\"fill:"+taskColor.get(task)+";stroke:black;stroke-width:3;"+
            "fill-opacity:1;stroke-opacity:1\"/>\n";
	
	
          plotString +="<text xml:space=\"preserve\" "+
            "style=\"font-size:"+fontsize+"px;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Bitstream Vera Sans\""+
            " x=\""+(currentTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30+blocksize+fontsize)+"\">"+(currentTime)+"</text>\n";
          if(finishTime != currentTime)
            plotString +="<text xml:space=\"preserve\" "+
              "style=\"font-size:"+fontsize+"px;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Bitstream Vera Sans\""+
              " x=\""+(finishTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30+blocksize+fontsize)+"\">"+(finishTime)+"</text>\n";
          //			" x=\""+(currentTime+startTime)+"\" y=\""+((task.RmPriority * blocksize)+ 30+blocksize+fontsize)+"\">"+(currentTime+startTime)+"</text>\n";
        }
    }

		
    return plotString;
  }

	
  public Vector<SlackRegion> calculateSlackVector(Task task, Iterable<Task> filter){
    Vector <SlackRegion> vector = new Vector<SlackRegion>();
    long previousExecutionRegion=0;
    long currentExecutionRegion=0;
    long virtualExecution=0;
    long nextTaskArrivalTime;
    Task nextArrivingTask;
    long nextSlackTime=0;
		
    do{
      boolean bootstrap=true;
      // next busy period
      do {
        previousExecutionRegion = currentExecutionRegion;
        if (bootstrap){
          // bootstrap next busy period 
          previousExecutionRegion += getInterferingTaskExecution(task,getNextInterferingNonZeroTaskArrival(task,filter,previousExecutionRegion, task.T));
          if (previousExecutionRegion == 0){
            vector.add(new SlackRegion(0,task.T));
            return vector;
          }
          bootstrap=false;
        }
        currentExecutionRegion=0;
        for (Task oTask:filter){
          currentExecutionRegion += (long)(Math.ceil(((double)previousExecutionRegion) / ((double)oTask.T)) * ((double)getInterferingTaskExecution(task,oTask)));
        }
        currentExecutionRegion += virtualExecution;
      } while (previousExecutionRegion < currentExecutionRegion && currentExecutionRegion < task.T);

      // end of busy period.
      nextArrivingTask =getNextArrivingTask(filter,currentExecutionRegion, task.T);
      nextTaskArrivalTime = (nextArrivingTask != null) ? ((long)(Math.ceil(((double)currentExecutionRegion)/((double)nextArrivingTask.T))*((double)nextArrivingTask.T))): task.T;

      if (nextTaskArrivalTime <= task.T)
        nextSlackTime = nextTaskArrivalTime - currentExecutionRegion;
      else 
        nextSlackTime = task.T - currentExecutionRegion;
      if (nextSlackTime >0){
        virtualExecution += nextSlackTime;
        vector.add(new SlackRegion(currentExecutionRegion, nextSlackTime));
        currentExecutionRegion += nextSlackTime;
      }
    } while(currentExecutionRegion < task.T);
    return vector;
  }

  long getLeadingSlackInstantInCMode(Task task, long slack, Vector<SlackRegion> slackV){
    long instant=0;
    int i=0;
    // FIXME: Check if always assuming C_c = epsilon is correct
    if(slack == 0)
      {
        if(slackV.size()==0)
          {
            return task.T;
          }
        else
          {
            return slackV.get(0).time;
          }
      }
    while(i<slackV.size() && slack>0){
      if (slack - slackV.get(i).slack <0){
        instant = (slackV.get(i).time+ slack);
        slack = 0;
      } else {
        slack -= slackV.get(i).slack;
        instant = slackV.get(i).time + slackV.get(i).slack;
      }
      i++;
    }
    if (slack >0)
      return task.T + slack;
    else
      return instant;
  }
	
  long getTrailingSlackInstant(Task task, long slack, Vector<SlackRegion> slackV){
    long instant=task.T;
    int i=slackV.size()-1;

    while(i>=0 && slack>0){
      if (slack - slackV.get(i).slack <0){
        instant = (slackV.get(i).time+slackV.get(i).slack) - slack;
        slack = 0;
      } else {
        slack -= slackV.get(i).slack;
        instant = slackV.get(i).time;
      }
      i--;
    }
    if (slack >0)
      return -1;
    else
      return instant;
  }
	
  public long getAvailableSlackUpToInstant(long instant, Vector<SlackRegion> slackV){
    long slack=0;
    for (int i=0;i<slackV.size() && slackV.get(i).time < instant;i++){
      if (slackV.get(i).time+slackV.get(i).slack > instant){
        slack += (instant - slackV.get(i).time);
        break;
      } else {
        slack += slackV.get(i).slack;
      }
    }
    return slack;
  }
	
  public long getCommonSlackOffsetUpToInstant(long offset,long computation, Vector<SlackRegion>rmSlackV,Vector<SlackRegion>cSlackV){
    long slack=0;
    long instant = offset+computation;
		
    for (int i=0; i<rmSlackV.size() && rmSlackV.get(i).time <= instant;i++){
      // check that we do not get outside the bounds
      if (rmSlackV.size() <=i || cSlackV.size() <= i)
        break;
      // same start?
      if (rmSlackV.get(i).time == cSlackV.get(i).time){
        // same end?
        if (rmSlackV.get(i).slack == cSlackV.get(i).slack){
          if (rmSlackV.get(i).time + rmSlackV.get(i).slack > instant){
            slack += instant - rmSlackV.get(i).time;
            break;							
          } else {
            slack += rmSlackV.get(i).slack;
          }
        } else {
          // cut it to the min of the two. In single mode cSlack subsumes rmSlack but in multimodal
          // this is not the case, hence we will double check.
          if (rmSlackV.get(i).slack < cSlackV.get(i).slack){
            if (rmSlackV.get(i).time + rmSlackV.get(i).slack > instant){
              slack += instant - rmSlackV.get(i).time;
            } else {
              slack += rmSlackV.get(i).slack;
            }
          } else {
            if (cSlackV.get(i).time + cSlackV.get(i).slack > instant){
              slack += instant - cSlackV.get(i).time;
            } else {
              slack += cSlackV.get(i).slack;
            }
          }
          break;
        }
      }
    }
    return slack;
  }
	
  public long getZeroSlackInstant(Task task, Vector<SlackRegion> rmSlackV, Vector<SlackRegion> cSlackV){
    long instant=0;
    long transferToRM=0;
		
    task.C_c = task.Ca; //(task.discount >= task.Ca)? 0:task.Ca - task.discount;
    task.C_rm = 0;
		
    do{
      if (cSlackV.size()>0){
        if (cSlackV.get(0).time >= task.discount){
          cSlackV.get(0).time -= task.discount;
          cSlackV.get(0).slack += task.discount;
        }
        else {
          int i=0;
        }
      } else {
        // this slack belongs to the end of the busy period
        // we need a way to accommodate this. This is not working.
        cSlackV.add(new SlackRegion(task.T-task.discount,task.discount));
      }
      if (rmSlackV.size() > 0){
        if (rmSlackV.get(0).time >= task.discount){
          rmSlackV.get(0).time -= task.discount;
          rmSlackV.get(0).slack += task.discount;
        }
      } else {
        rmSlackV.add(new SlackRegion(task.T-task.discount,task.discount));
      }
      instant = getLeadingSlackInstantInCMode(task,task.C_c,cSlackV);
      if (instant <=task.T){
        transferToRM = getAvailableSlackUpToInstant(task.T-instant,rmSlackV) - task.C_rm;
        // This is a potential slack that we still need to prove
        //				if (transferToRM > -task.C_rm) 
        //					transferToRM+= task.discount;
        if (transferToRM <0){
          transferToRM = 0;
        } else {
          if (transferToRM == 0){
            // is there any common slack that I can transfer from C to RM
            transferToRM = getCommonSlackOffsetUpToInstant(task.T-instant,task.C_c,rmSlackV,cSlackV);
          }
          if (transferToRM > task.C_c){
            transferToRM = task.C_c;
          }
          task.C_c -= transferToRM;
          task.C_rm += transferToRM;
        }
      } else {
        transferToRM=0;
      }
    } while(transferToRM != 0);
    return task.T-instant;
  }
	
  public void compressSchedule(){
    boolean sameZsInstants=false;
    long [] prevZsInstant = new long[tasksByPriority.size()];
    long [] currZsInstant = new long[tasksByPriority.size()];
		
    for (int i=0;i<prevZsInstant.length;i++){
      prevZsInstant[i]=0;
      currZsInstant[i]=0;
    }

    // initialize execution zones
    for (Task task:tasksByPriority){
      DualExecutionZone deZone = new DualExecutionZone();
      executionZones.put(task,deZone);
    }
		
		
    do {
      for (int i=0;i<prevZsInstant.length;i++){
        prevZsInstant[i] = currZsInstant[i];
      }			

      int index=0;
      for (Task task:tasksByCriticality){
        executionZones.get(task).cSlackVector = calculateSlackVector(task, new CriticalZoneTaskIterable(tasksByPriority,task));
        executionZones.get(task).rmSlackVector = calculateSlackVector(task, new RMZoneTaskIterable(tasksByPriority,task));
        //				long optimisticDiscount=0;
        //				for (Task taskj:tasksByCriticality){
        //					if (taskj.equals(task))
        //						break;
        //					if(taskj.RmPriority <= task.RmPriority)
        //						continue;
        //					
        //					Vector<SlackRegion> v = calculateOptimisticSlack(task, taskj,new RMZoneTaskIterable(tasksByPriority,taskj));
        //					long localDiscount = getAvailableSlackUpToInstant(taskj.ZeroSlackInstant,v);
        //					if (localDiscount > taskj.Cs)
        //						localDiscount = taskj.Cs;
        //					optimisticDiscount += localDiscount; 
        //					
        //				}
        //				task.discount = optimisticDiscount;
        currZsInstant[index] = getZeroSlackInstant(task, executionZones.get(task).rmSlackVector, executionZones.get(task).cSlackVector);
        task.ZeroSlackInstant =currZsInstant[index]; 
        index++;
      }
			
      // calculate finishing condition
      sameZsInstants=true;
      for (int i=0;i<prevZsInstant.length;i++){
        sameZsInstants = sameZsInstants && (prevZsInstant[i] == currZsInstant[i]);
      }
    } while(!sameZsInstants);
		
    int i=0;
    for (Task task:tasksByCriticality){
      task.ZeroSlackInstant = currZsInstant[i];
      i++;
    }		
  }
	
  public void svgPlotSlackVectors(String namefile){
		
    try{
      FileWriter fw = new FileWriter(namefile);
      fw.write("<?xml version=\"1.0\" standalone=\"no\"?>\n"+
               "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"+ 
               "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n"+
               "<svg width=\"200000\" height=\"300\" version=\"1.1\"\n"+
               "xmlns=\"http://www.w3.org/2000/svg\">\n");

      String [] colors= new String[]{"red","green","blue","white","gray","black"};
      int i=0;
      int y=0;
      for (Task t:tasksByPriority){
        String color = colors[i];
        i = (i+1)%colors.length;
        String s = executionZones.get(t).slackVectorToSVGString(executionZones.get(t).rmSlackVector, y, 1, 50, color, 12);
        y=y+80;
        fw.write(s);
      }
			
      fw.write("</svg>\n");
      fw.flush();
      fw.close();
    } catch (Exception e){
      e.printStackTrace();
    }
		
  }
	
  public void svgPlotCriticalInstant(Task task, String filename){
    String rmZonePlot=null;
    String cZonePlot = null;

    // initialize the remaining computation of all tasks
    // so we can keep the remaining across calls to svgPlotExecutionRegion()
    for (Task t:tasksByPriority){
      t.remainingComputation=0;
    }
    task.remainingComputation = task.Ca;
    rmZonePlot = svgPlotExecutionRegion(task, new RMZoneTaskIterable(tasksByPriority,task),0,task.ZeroSlackInstant);
    cZonePlot = svgPlotExecutionRegion(task, new CriticalZoneTaskIterable(tasksByPriority,task),task.ZeroSlackInstant,task.T);

    try{
      FileWriter fw = new FileWriter(filename);
      fw.write("<?xml version=\"1.0\" standalone=\"no\"?>\n"+
               "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"+ 
               "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n"+
               "<svg width=\""+(task.T+100)+"\" height=\""+(tasksByPriority.size()*100)+"\" version=\"1.1\"\n"+
               "xmlns=\"http://www.w3.org/2000/svg\">\n");

      fw.write(rmZonePlot+"\n");
      fw.write(cZonePlot+"\n");
			
      fw.write("</svg>\n");
      fw.flush();
      fw.close();
    } catch (Exception e){
      e.printStackTrace();
    }
		
  }
	
  public static void main(String args[]){
    ZSRMScheduler schedHostiles = new ZSRMScheduler();
    ZSRMScheduler schedFriendlies = new ZSRMScheduler();

    // Not overloaded		
    //											T			Ca		Cs		C	P				
    //		Task t0 = new Task("TrackGen", 		100000, 	450, 	450, 	0, 	0);
    //		Task t1 = new Task("TrackSender", 	125000, 	220, 	220, 	1, 	3);
    //		Task t2 = new Task("SearchTask", 	1000000, 	84896, 	84896, 	2, 	6);
    //		Task t3 = new Task("HP_Hostile", 	100000, 	7542, 	7542, 	3, 	1);
    //		Task t4 = new Task("NP_Hostile", 	250000, 	17858, 	17858, 	4, 	4);
    //		Task t5 = new Task("HP_Friendly", 	100000, 	2010, 	2010, 	5, 	2);
    //		Task t6 = new Task("NP_Friendly", 	250000, 	6418, 	6418, 	6, 	5);

    // 		Overloaded		
    //											T			Ca		Cs		C	P				
    //		T			Ca		Cs		C	P				
    //		Task t0 = new Task("TrackGen", 		100000, 	448, 	448, 	0, 	0);
    //		Task t1 = new Task("TrackSender", 	125000, 	219, 	219, 	1, 	3);
    //		Task t2 = new Task("SearchTask", 	1000000, 	84896, 	84896, 	2, 	6);
    //		Task t3 = new Task("HP_Hostile", 	100000, 	37710, 	7542, 	3, 	1);
    //		Task t4 = new Task("NP_Hostile", 	250000, 	89290, 	17858, 	4, 	4);
    //		Task t5 = new Task("HP_Friendly", 	100000, 	8040, 	2010, 	5, 	2);
    //		Task t6 = new Task("NP_Friendly", 	250000, 	32090, 	6418, 	6, 	5);

    //											T			Ca			Cs			C	P				
    //		Task t0 = new Task("TrackGen", 		100000, 	2704, 		1082, 		0, 	0);
    //		Task t1 = new Task("TrackSender", 	125000, 	94, 		94, 		1, 	3);
    //		Task t2 = new Task("SearchTask", 	1000000, 	320818, 	128327, 	2, 	6);
    //		Task t3 = new Task("HP_Hostile", 	100000, 	22531, 		9012, 		3, 	1);
    //		Task t4 = new Task("NP_Hostile", 	250000, 	96865, 		38746, 		4, 	4);//31745, 		4, 	4);
    //		Task t5 = new Task("HP_Friendly", 	100000, 	4540, 		4540, 		5, 	2);
    //		Task t6 = new Task("NP_Friendly", 	250000, 	25367, 		25367, 		6, 	5);

    //		Task t0 = new Task("TrackGen", 		100000, 	2704, 		1082, 		0, 	0);
    //		Task t1 = new Task("TrackSender", 	125000, 	94, 		94, 		1, 	3);
    //		Task t2 = new Task("SearchTask", 	1000000, 	320818, 	128327, 	2, 	6);

    //											T			Ca			Cs			C	P				
    Task t3 = new Task("HP_Hostile", 	100000, 	15704, 		15407, 		3, 	1);
    Task t4 = new Task("NP_Hostile", 	250000, 	160268,		129249, 	4, 	4);//31745, 		4, 	4);
    Task t5 = new Task("HP_Friendly", 	100000, 	15714, 		15415, 		5, 	2);
    Task t6 = new Task("NP_Friendly", 	250000, 	159597,		128708, 	6, 	5);
		
    //		sched.addTask(t0);
    //		sched.addTask(t1);
    //		sched.addTask(t2);
		
    // Mixed Criticality Allocation
    schedHostiles.addTask(t3);
    schedHostiles.addTask(t6);
    schedFriendlies.addTask(t5);
    schedFriendlies.addTask(t4);

    // Isolated Criticalities
    //		schedHostiles.addTask(t3);
    //		schedHostiles.addTask(t4);
    //		schedFriendlies.addTask(t5);
    //		schedFriendlies.addTask(t6);

    //		Task t0 = new Task("t0", 100, 50, 10, 2, 0);
    //		Task t1 = new Task("t1", 200, 100, 20, 1, 1);
    //		Task t2 = new Task("t2", 400, 200, 40, 0, 2);
    //
    //		sched.addTask(t0);
    //		sched.addTask(t1);
    //		sched.addTask(t2);
		
    //		int NumT0 = 1;
    //		int NumT1 = 1;
    //		int NumT2 = 1;
    //		int NumT3 = 25;
    //		int NumT4 = 11;
    //		int NumT5 = 4;
    //		
    //		Task t0 = new Task("t0",1000000, 90000*NumT0, 90000*NumT0, 0, 5);
    //		Task t1 = new Task("t1",500000, 2000*NumT1, 2000*NumT1, 1, 4);
    //		Task t2 = new Task("t2",100000, 6000*NumT2, 1000*NumT2, 2, 1);
    //		Task t3 = new Task("t3",250000, 6000*NumT3, 1000*NumT3, 3, 3);
    //		Task t4 = new Task("t4",100000, 6000*NumT4, 1000*NumT4, 4, 0);
    //		Task t5 = new Task("t5",250000, 6000*NumT5, 1000*NumT5, 5, 2);
    //		
    //		sched.addTask(t0);
    //		sched.addTask(t1);
    //		sched.addTask(t2);
    //		sched.addTask(t3);
    //		sched.addTask(t4);
    //		sched.addTask(t5);
		
    //		Task t0 = new Task(40,20,20,1,0);
    //		Task t1 = new Task(80,50,25,0,1);
    //		sched.addTask(t0);
    //		sched.addTask(t1);

    //Task t0 = new Task(1000, 550, 400, 2, 0);
    //		Task t0 = new Task(1000, 100, 100, 2, 0);
    //		Task t1 = new Task(4200, 1200, 300, 1, 1);
    //		Task t2 = new Task(9400, 4000, 3100, 0, 2);
		
		
    //		int NumT0=1;
    //		int NumT1=1;
    //		int NumT2=1;
    //		int NumT3=1;
    //		int NumT4=1;
    //		int NumT5=1;
    //		int NumT6=1;
    //		
    //		Task t0 = new Task(100000,90*NumT0,90*NumT0,0,5);
    //		Task t1 = new Task(50000,90*NumT0,90*NumT0,0,5);
		
		
    schedHostiles.compressSchedule();		

    /*
      for (Task task:schedHostiles.tasksByPriority){
      schedHostiles.svgPlotCriticalInstant(task, "c:/temp/critInstant"+task.name+"Hostiles.svg");
      }
    */
		
    System.out.println("Hostile Scheduled Tasks:");
    for (Task t:schedHostiles.tasksByPriority){
      System.out.println("Task ["+t.name+"] (T="+t.T+") Crm("+t.C_rm+") Cc("+t.C_c+"): ZS:"+t.ZeroSlackInstant+" discount: "+t.discount);			
    }

    schedFriendlies.compressSchedule();		

    /*
      for (Task task:schedFriendlies.tasksByPriority){
      schedHostiles.svgPlotCriticalInstant(task, "c:/temp/critInstant"+task.name+"Friendlies.svg");
      }
    */
		
    System.out.println("Friendlies Scheduled Tasks:");
    for (Task t:schedFriendlies.tasksByPriority){
      System.out.println("Task ["+t.name+"] (T="+t.T+") Crm("+t.C_rm+") Cc("+t.C_c+"): ZS:"+t.ZeroSlackInstant+" discount: "+t.discount);			
    }
		
    //		Vector <SlackRegion> vector = sched.calculateSlackVector(t1, new RMZoneTaskIterable(sched.tasksByPriority,t1));
    //		Vector <SlackRegion> vector = sched.calculateSlackVector(t2, new RMZoneTaskIterable(sched.tasksByPriority,t2));
    //		Vector <SlackRegion> vector1 = sched.calculateSlackVector(t2, new RMZoneTaskIterable(sched.tasksByPriority,t2));
    //
    //		Vector <SlackRegion> vector = sched.calculateSlackVector(t2, new CriticalZoneTaskIterable(sched.tasksByPriority,t2));
    //		
    //		System.out.println("Critical Slack vector of t2");
    //		for (SlackRegion r:vector){
    //			System.out.println("Slack[ time: "+r.time+", slack: "+r.slack+"]");
    //		}
    //
    //		System.out.println("RM Slack vector of t1");		
    //		for (SlackRegion r:vector1){
    //			System.out.println("Slack[ time: "+r.time+", slack: "+r.slack+"]");
    //		}
  }

  //-- the argument is a comma separated list X1,X2, ...,Xn where each
  //-- Xi is a task desciptor of the form A:B:C:D:E:F where
  //-- A is the task name
  //-- B is the period
  //-- C is the overloaded WCET
  //-- D is the normal WCET
  //-- E is the criticality (lower number means higher criticality)
  //-- F is the priority (lower number means higher priority)
  //-- the return value is a comma separated list of zero slack instants
  public static String computeZSInstants(String arg)
  {
    //System.out.println(arg);

    //-- create tasks
    ZSRMScheduler sched = new ZSRMScheduler();
    for(String x : arg.split(",")) {
      String [] y = x.split(":");
      sched.addTask(new Task(y[0], Long.parseLong(y[1]), Long.parseLong(y[2]),
                             Long.parseLong(y[3]), Long.parseLong(y[4]),
                             Long.parseLong(y[5])));
    }

    //-- run schedulability analysis
    sched.compressSchedule();		

    //-- construct and return result
    String res = null;
    for (Task t : sched.tasksByPriority) {
      Long zsi = new Long(t.ZeroSlackInstant);
      if(res == null) res = t.name + ":" + zsi.toString();
      else res = res + "," + t.name + ":" + zsi.toString();
    }

    return res;
  }
}
