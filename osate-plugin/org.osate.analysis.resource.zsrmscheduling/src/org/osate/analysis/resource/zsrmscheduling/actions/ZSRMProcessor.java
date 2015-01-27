package org.osate.analysis.resource.zsrmscheduling.actions;

import org.osate.aadl2.instance.ComponentInstance;

public class ZSRMProcessor {

	static long nextId = 0;

	long id = nextId++;

	public long getUniqueId() {
		return id;
	}

	ComponentInstance instance;

	public static ZSRMProcessor createInstance(ComponentInstance ci) {
		ZSRMProcessor proc = new ZSRMProcessor();
		proc.instance = ci;
		return proc;
	}

}
