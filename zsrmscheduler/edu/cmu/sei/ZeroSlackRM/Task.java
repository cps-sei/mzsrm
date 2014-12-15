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

public class Task {
	static long nextUniqueID=0;
	long uniqueID = nextUniqueID++;
	
	long T;
	long Ca;
	long Cs;
	long C_rm=0;
	long C_c=0;
	public long criticality=0;
	public long RmPriority=0;
	public long ZeroSlackInstant=0;
	public long discount=0;
	public long nextArrival=0;
	public long remainingComputation=0;
	public String name;

	public long getUniqueID(){
		return uniqueID;
	}
	
	public long getCc(){
		return C_c;
	}
	
	public long getCrm(){
		return C_rm;
	}
	
	public long getPeriod(){
		return T;
	}
	
	public long getOverloadedC(){
		return Ca;
	}
	
	public long getNormalC(){
		return Cs;
	}
	
	public Task(String name, long T, long Ca, long Cs, long criticality, long RMPriority){
		this.T = T;
		this.Ca = Ca;
		this.Cs = Cs;
		this.name = name;
		this.criticality = criticality;
		this.RmPriority = RMPriority;
		ZeroSlackInstant = T;
	}
	
	public long getC(Task preemptedTask){
		long C=0;
		
		return C;
	}
}
