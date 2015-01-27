package org.osate.analysis.resource.zsrmscheduling.actions;

import java.io.IOException;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.TreeSet;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.emf.common.command.Command;
import org.eclipse.emf.transaction.RecordingCommand;
import org.eclipse.emf.transaction.RollbackException;
import org.eclipse.emf.transaction.TransactionalCommandStack;
import org.eclipse.emf.transaction.TransactionalEditingDomain;
import org.eclipse.jface.dialogs.MessageDialog;
import org.osate.aadl2.Aadl2Package;
import org.osate.aadl2.ComponentCategory;
import org.osate.aadl2.ContainmentPathElement;
import org.osate.aadl2.Element;
import org.osate.aadl2.IntegerLiteral;
import org.osate.aadl2.ModalPropertyValue;
import org.osate.aadl2.NamedElement;
import org.osate.aadl2.Property;
import org.osate.aadl2.PropertyAssociation;
import org.osate.aadl2.Subcomponent;
import org.osate.aadl2.instance.ComponentInstance;
import org.osate.aadl2.instance.SystemInstance;
import org.osate.aadl2.instance.SystemOperationMode;
import org.osate.aadl2.modelsupport.errorreporting.AnalysisErrorReporterManager;
import org.osate.aadl2.modelsupport.modeltraversal.ForAllElement;
import org.osate.ui.actions.AbstractInstanceOrDeclarativeModelReadOnlyAction;
import org.osate.xtext.aadl2.properties.linking.PropertiesLinkingService;
import org.osate.xtext.aadl2.properties.util.GetProperties;

import edu.cmu.sei.ZeroSlackRM.IncreasingPeriodComparator;
import edu.cmu.sei.ZeroSlackRM.Task;
import edu.cmu.sei.ZeroSlackRM.ZSRMScheduler;

public class ZSRMSchedulingAction extends AbstractInstanceOrDeclarativeModelReadOnlyAction {

	protected void analyzeDeclarativeModel(IProgressMonitor monitor, AnalysisErrorReporterManager errManager,
			Element declarativeObject) {
		// TODO Auto-generated method stub

	}

	ArrayList<ZSRMProcessor> processors = new ArrayList<ZSRMProcessor>();
	HashMap<ComponentInstance, ZSRMProcessor> instance2processor = new HashMap<ComponentInstance, ZSRMProcessor>();
	HashMap<ZSRMProcessor, ArrayList<ZSRMTask>> proc2Taskset = new HashMap<ZSRMProcessor, ArrayList<ZSRMTask>>();

	ArrayList<ZSRMTask> tasks = new ArrayList<ZSRMTask>();
	HashMap<Task, ZSRMTask> task2zsrmtask = new HashMap<Task, ZSRMTask>();

	protected void analyzeInstanceModel(IProgressMonitor monitor, AnalysisErrorReporterManager errManager,
			final SystemInstance root, SystemOperationMode som) {

		processors.clear();
		instance2processor.clear();
		tasks.clear();
		task2zsrmtask.clear();

		// get processors
		final ForAllElement addProcessors = new ForAllElement(errManager) {
			public void process(Element obj) {
				ComponentInstance ci = (ComponentInstance) obj;
				final ZSRMProcessor proc = ZSRMProcessor.createInstance(ci);
				processors.add(proc);
				instance2processor.put(ci, proc);
			}
		};
		addProcessors.processPreOrderComponentInstance(root, ComponentCategory.PROCESSOR);

		// get processors
		final ForAllElement addTasks = new ForAllElement(errManager) {
			public void process(Element obj) {
				ComponentInstance ci = (ComponentInstance) obj;
				final ZSRMTask task = ZSRMTask.createInstance(ci);
				tasks.add(task);

				List<ComponentInstance> pciList = GetProperties.getActualProcessorBinding(ci);

				if (pciList.size() == 1) {
					ZSRMProcessor proc = instance2processor.get(pciList.get(0));
					if (proc != null) {
						ArrayList<ZSRMTask> taskset = proc2Taskset.get(proc);
						if (taskset == null) {
							taskset = new ArrayList<ZSRMTask>();
							proc2Taskset.put(proc, taskset);
						}
						taskset.add(task);
					}
				}
			}
		};
		addTasks.processPreOrderComponentInstance(root, ComponentCategory.THREAD);

		final ZSRMScheduler sched = new ZSRMScheduler();

		TreeSet<ZSRMTask> prioritylist = new TreeSet<ZSRMTask>(new IncreasingPeriodComparator());
		prioritylist.addAll(tasks);

		int priority = 1;

		for (ZSRMTask t : prioritylist) {
			t.setPriority(priority++);
		}

		for (ZSRMTask task : tasks) {
			Task t = new Task("", task.getPeriodNanos(), task.getOverloadedWCETNanos(), task.getNominalWCETNanos(),
					task.getCriticality(), task.getPriority());
			sched.addTask(t);
			task2zsrmtask.put(t, task);
			System.out.println("Task: period[" + task.getPeriodNanos() + "] Co[" + task.getOverloadedWCETNanos()
					+ "] C[" + task.getNominalWCETNanos() + "] criticality[" + task.getCriticality() + "] priority["
					+ task.getPriority() + "]");
		}

		sched.compressSchedule();

		boolean schedulable = true;
		for (Task t : sched.getTasksByPriority()) {
			schedulable = schedulable && (t.ZeroSlackInstant >= 0);
		}

		final boolean isSchedulable = schedulable;

		getShell().getDisplay().syncExec(new Runnable() {
			@Override
			public void run() {
				String schedString = (isSchedulable) ? "The System is Schedulable" : "The System is NOT schedulable";
				MessageDialog.openInformation(getShell(), "Results", schedString);
			}
		});

		if (schedulable) {
			final TransactionalEditingDomain domain = TransactionalEditingDomain.Registry.INSTANCE
					.getEditingDomain("org.osate.aadl2.ModelEditingDomain");
			// We execute this command on the command stack because otherwise, we will not
			// have write permissions on the editing domain.
			Command cmd = new RecordingCommand(domain) {

				@Override
				protected void doExecute() {
					Property propZS = GetProperties.lookupPropertyDefinition(root.getComponentImplementation(),
							"Zero_Slack_Scheduling", "Zero_Slack_Instant");

					for (Task t : sched.getTasksByPriority()) {
						PropertyAssociation paZS = root.getComponentImplementation().createOwnedPropertyAssociation();
						paZS.setProperty(propZS);
						ModalPropertyValue pvZS = paZS.createOwnedValue();

						IntegerLiteral intLiteralZS = (IntegerLiteral) pvZS.createOwnedValue(Aadl2Package.eINSTANCE
								.getIntegerLiteral());
						intLiteralZS.setValue(t.ZeroSlackInstant);
						intLiteralZS.setUnit(PropertiesLinkingService.findUnitLiteral(propZS, "ns"));
						ArrayDeque<Subcomponent> containmentPath = new ArrayDeque<Subcomponent>();
						ZSRMTask zsrmtask = task2zsrmtask.get(t);
						for (ComponentInstance parent = zsrmtask.instance; parent != root; parent = parent
								.getContainingComponentInstance()) {
							containmentPath.push(parent.getSubcomponent());
						}
						if (!containmentPath.isEmpty()) {
							NamedElement ne = containmentPath.pop();
							ContainmentPathElement peZS = paZS.createAppliesTo().createPath();
							peZS.setNamedElement(ne);
							while (!containmentPath.isEmpty()) {
								ne = containmentPath.pop();
								peZS = peZS.createPath();
								peZS.setNamedElement(ne);
							}
						}
					}

				}
			};

			try {
				((TransactionalCommandStack) domain.getCommandStack()).execute(cmd, null);
			} catch (InterruptedException | RollbackException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			try {
				root.getComponentImplementation().eResource().save(null);
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}

	}

	protected String getActionName() {
		return "ZSRMScheduler";
	}

}
