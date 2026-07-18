// Vaccination schedule/compliance derivation (US-VAC-002/004). Pure
// functions — the schedule itself is never stored, only the template
// items and the records; due/overdue status is computed on every read
// so it's never stale.

export interface TemplateItemRef {
  id: string;
  ageDaysFrom: number;
  ageDaysTo: number;
  vaccine: string;
  disease: string;
  route: string;
}

export type ComplianceStatus = "administered" | "overdue" | "due" | "upcoming";

export interface ComplianceItem extends TemplateItemRef {
  dueDate: Date;
  status: ComplianceStatus;
  record: { id: string; date: Date } | null;
}

// US-VAC-002: reminders start this many hours before the due date.
export const DUE_LEAD_HOURS = 24;

export function computeCompliance(
  birth: Date,
  templates: TemplateItemRef[],
  records: { id: string; templateItemId: string | null; date: Date }[],
  now: Date = new Date(),
): ComplianceItem[] {
  const recordByTemplate = new Map(records.filter((r) => r.templateItemId).map((r) => [r.templateItemId as string, r]));

  return templates
    .map((t) => {
      const dueDate = new Date(birth.getTime() + t.ageDaysFrom * 86_400_000);
      const record = recordByTemplate.get(t.id) ?? null;
      let status: ComplianceStatus;
      if (record) {
        status = "administered";
      } else if (dueDate.getTime() <= now.getTime()) {
        status = "overdue";
      } else if (dueDate.getTime() - now.getTime() <= DUE_LEAD_HOURS * 3_600_000) {
        status = "due";
      } else {
        status = "upcoming";
      }
      return { ...t, dueDate, status, record: record ? { id: record.id, date: record.date } : null };
    })
    .sort((a, b) => a.dueDate.getTime() - b.dueDate.getTime());
}
