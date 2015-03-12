package org.osate.analysis.resource.dmpl.generator.actions;

import java.io.FileNotFoundException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;

import org.eclipse.core.runtime.IProgressMonitor;
import org.osate.aadl2.CalledSubprogram;
import org.osate.aadl2.ComponentCategory;
import org.osate.aadl2.ContainmentPathElement;
import org.osate.aadl2.Element;
import org.osate.aadl2.Property;
import org.osate.aadl2.PropertyExpression;
import org.osate.aadl2.ReferenceValue;
import org.osate.aadl2.Subprogram;
import org.osate.aadl2.SubprogramCall;
import org.osate.aadl2.SubprogramCallSequence;
import org.osate.aadl2.instance.ComponentInstance;
import org.osate.aadl2.instance.SystemInstance;
import org.osate.aadl2.instance.SystemOperationMode;
import org.osate.aadl2.modelsupport.errorreporting.AnalysisErrorReporterManager;
import org.osate.aadl2.modelsupport.modeltraversal.ForAllElement;
import org.osate.aadl2.properties.PropertyNotPresentException;
import org.osate.analysis.resource.zsrmscheduling.actions.ZSRMTask;
import org.osate.ui.actions.AbstractInstanceOrDeclarativeModelReadOnlyAction;
import org.osate.xtext.aadl2.properties.util.GetProperties;
import org.osate.xtext.aadl2.properties.util.PropertyUtils;

public class DMPLGeneratorAction extends AbstractInstanceOrDeclarativeModelReadOnlyAction {

	@Override
	protected void analyzeDeclarativeModel(IProgressMonitor monitor, AnalysisErrorReporterManager errManager,
			Element declarativeObject) {
	}

	public String getDMPLTimingParametersFileName(ComponentInstance system) {
		Property sourceNameProp = GetProperties.lookupPropertyDefinition(system, "DART", "DMPL_TIMING_PARAMETERS_TEXT");
		String fileName = PropertyUtils.getStringValue(system, sourceNameProp);
		return fileName;
	}

	public String getMainSubprogram(ComponentInstance threadInstance) {
		String progName = "";
		try {
			Property entrycallsProperty = GetProperties.lookupPropertyDefinition(threadInstance,
					"PROGRAMMING_PROPERTIES", "COMPUTE_ENTRYPOINT_CALL_SEQUENCE");

			PropertyExpression propertyValue = threadInstance.getSimplePropertyValue(entrycallsProperty);
			ReferenceValue callseqref = (ReferenceValue) propertyValue;
			ContainmentPathElement element = callseqref.getContainmentPathElements().get(0);
			SubprogramCallSequence cs = (SubprogramCallSequence) element.getNamedElement();

			SubprogramCall call = cs.getOwnedSubprogramCalls().get(0);

			CalledSubprogram called = call.getCalledSubprogram();
			Subprogram subprogram = (Subprogram) called;

			Property sourceNameProp = GetProperties.lookupPropertyDefinition(subprogram, "PROGRAMMING_PROPERTIES",
					"SOURCE_NAME");

			progName = PropertyUtils.getStringValue(subprogram, sourceNameProp);
		} catch (PropertyNotPresentException pne) {
			System.out.println(threadInstance.getName() + " has no Source_Name property");
		} catch (Exception e) {
			e.printStackTrace();
		}

		return progName;
	}

	ArrayList<ZSRMTask> tasks = new ArrayList<ZSRMTask>();
	ArrayList<ComponentInstance> processes = new ArrayList<ComponentInstance>();
	HashMap<ComponentInstance, ArrayList<ZSRMTask>> proc2taskset = new HashMap<ComponentInstance, ArrayList<ZSRMTask>>();

	protected void analyzeInstanceModel(IProgressMonitor monitor, final AnalysisErrorReporterManager errManager,
			SystemInstance root, SystemOperationMode som) {

		tasks.clear();
		processes.clear();
		proc2taskset.clear();

		final ForAllElement addProcesses = new ForAllElement(errManager) {
			public void process(Element obj) {
				ComponentInstance procinst = (ComponentInstance) obj;
				processes.add(procinst);
				final ArrayList<ZSRMTask> taskset = new ArrayList<ZSRMTask>();
				proc2taskset.put(procinst, taskset);
				final ForAllElement addTasks = new ForAllElement(errManager) {
					public void process(Element obj) {
						ComponentInstance ci = (ComponentInstance) obj;
						final ZSRMTask task = ZSRMTask.createInstance(ci);
						task.fillZeroSlackInstant();
						tasks.add(task);
						taskset.add(task);
					}
				};
				addTasks.processPreOrderComponentInstance(procinst, ComponentCategory.THREAD);
			}
		};
		addProcesses.processPreOrderComponentInstance(root, ComponentCategory.PROCESS);

		String filename = getDMPLTimingParametersFileName(root);

		try {
			PrintWriter pw = new PrintWriter(filename);
			for (ComponentInstance process : processes) {
				pw.println("NODE  uav"); // + process.getContainingComponentInstance().getName());
				pw.println("{");
				ArrayList<ZSRMTask> taskset = proc2taskset.get(process);
				// for (ZSRMTask t : tasks) {
				for (ZSRMTask t : taskset) {
					ComponentInstance instance = t.getComponentInstance();
					String progName = getMainSubprogram(instance);
					// pw.println("@PERIOD(" + t.getPeriodNanos() + ")");
					pw.println("@HERTZ(" + (int) (1000000000.0 / (t.getPeriodNanos() * 1.0)) + ")");
					pw.println("@CRITICALITY(" + t.getCriticality() + ")");
					pw.println("@WCET_NOMINAL(" + (t.getNominalWCETNanos() / 1000.0) + ")");
					pw.println("@WCET_OVERLOAD(" + (t.getOverloadedWCETNanos() / 1000.0) + ")");
					pw.println("@ZERO_SLACK_INSTANT(" + (t.getZeroSlackInstantNanos() / 1000.0) + ")");
					pw.println(progName + "();");
					pw.println(" ");
				}
				pw.println("}");

				// **** HACK !!!!!!!*******
				// For now we only generate the first node
				break;
			}
			pw.close();
		} catch (FileNotFoundException e) {
			System.out.println("Filename [" + filename + "] not valid");
		}

	}

	protected String getActionName() {
		return "DMPLGenerator";
	}

}
