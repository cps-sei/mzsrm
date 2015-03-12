package org.osate.analysis.resource.zsrmscheduling.actions;

import org.osate.aadl2.Property;
import org.osate.aadl2.UnitLiteral;
import org.osate.aadl2.instance.ComponentInstance;
import org.osate.xtext.aadl2.properties.util.AadlProject;
import org.osate.xtext.aadl2.properties.util.GetProperties;
import org.osate.xtext.aadl2.properties.util.PropertyUtils;
import org.osate.xtext.aadl2.properties.util.TimingProperties;

public class ZSRMTask {

	public void fillExecutionTime() {
		Property computeExecutionTime = GetProperties.lookupPropertyDefinition(instance, TimingProperties._NAME,
				TimingProperties.COMPUTE_EXECUTION_TIME);
		UnitLiteral second = GetProperties.findUnitLiteral(computeExecutionTime, AadlProject.NS_LITERAL);
		double mintime = PropertyUtils.getScaledRangeMinimum(instance, computeExecutionTime, second, 0.0);
		double maxtime = PropertyUtils.getScaledRangeMaximum(instance, computeExecutionTime, second, 0.0);
		nominalWCETNanos = (long) mintime;
		overloadedWCETNanos = (long) maxtime;
	}

	public void fillCriticality() {
		Property criticalityProperty = GetProperties.lookupPropertyDefinition(instance, "Zero_Slack_Scheduling",
				"Criticality");
		criticality = (int) PropertyUtils.getIntegerValue(instance, criticalityProperty);
	}

	public void fillZeroSlackInstant() {
		Property zeroSlackInstantProperty = GetProperties.lookupPropertyDefinition(instance, "Zero_Slack_Scheduling",
				"Zero_Slack_Instant");
		UnitLiteral nanoSecond = GetProperties.findUnitLiteral(zeroSlackInstantProperty, AadlProject.NS_LITERAL);
		zeroSlackInstantNanos = (long) PropertyUtils.getScaledNumberValue(instance, zeroSlackInstantProperty,
				nanoSecond);
	}

	static long nextId = 0;

	long id = nextId++;

	public long getUniqueId() {
		return id;
	}

	ComponentInstance instance;

	public ComponentInstance getComponentInstance() {
		return instance;
	}

	public static ZSRMTask createInstance(ComponentInstance ci) {
		ZSRMTask task = new ZSRMTask();
		task.instance = ci;
		task.periodNanos = (long) GetProperties.getPeriodinNS(ci);
		task.fillExecutionTime();
		task.fillCriticality();
		return task;
	}

	long zeroSlackInstantNanos;

	public long getZeroSlackInstantNanos() {
		return zeroSlackInstantNanos;
	}

	public void setZeroSlackInstantNanos(long z) {
		zeroSlackInstantNanos = z;
	}

	long periodNanos;

	public long getPeriodNanos() {
		return periodNanos;
	}

	public void setPeriodNanos(long p) {
		periodNanos = p;
	}

	long overloadedWCETNanos;

	public long getOverloadedWCETNanos() {
		return overloadedWCETNanos;
	}

	long nominalWCETNanos;

	public long getNominalWCETNanos() {
		return nominalWCETNanos;
	}

	int criticality;

	public int getCriticality() {
		return criticality;
	}

	int priority;

	public int getPriority() {
		return priority;
	}

	public void setPriority(int p) {
		priority = p;
	}
}
