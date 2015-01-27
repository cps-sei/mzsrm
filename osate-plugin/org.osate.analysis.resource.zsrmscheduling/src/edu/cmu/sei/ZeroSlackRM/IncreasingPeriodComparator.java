package edu.cmu.sei.ZeroSlackRM;

import java.util.Comparator;
import java.util.TreeSet;

import org.osate.analysis.resource.zsrmscheduling.actions.ZSRMTask;

public class IncreasingPeriodComparator implements Comparator<ZSRMTask> {

	public int compare(ZSRMTask t0, ZSRMTask t1) {
		if (t0.getUniqueId() == t1.getUniqueId())
			return 0;

		if (t0.getPeriodNanos() < t1.getPeriodNanos())
			return -1;
		if (t0.getPeriodNanos() > t1.getPeriodNanos())
			return 1;

		return (int) (t0.getUniqueId() - t1.getUniqueId());
	}

	// test
	public static void main(String args[]) {
		ZSRMTask t0 = new ZSRMTask();
		ZSRMTask t1 = new ZSRMTask();

		t0.setPeriodNanos(1);
		t1.setPeriodNanos(2);

		TreeSet<ZSRMTask> tasks = new TreeSet<ZSRMTask>(new IncreasingPeriodComparator());

		tasks.add(t0);
		tasks.add(t1);

		for (ZSRMTask t : tasks) {
			System.out.println(t.getPeriodNanos());
		}
	}
}
