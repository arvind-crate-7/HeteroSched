import { useState } from 'react';
import { activeSimulationResult, benchmarkSummary } from './data/simulationData.ts';
import { TaskRecord, CanonicalEvent, BenchmarkRecord } from './types/index.ts';

export default function App() {
  const [activeTab, setActiveTab] = useState<'timeline' | 'tasks' | 'comparison' | 'events'>('timeline');
  const [selectedTask, setSelectedTask] = useState<TaskRecord | null>(null);
  const [eventFilter, setEventFilter] = useState<string>('');

  const sim = activeSimulationResult;
  const m = sim.metrics;
  const cfg = sim.config;

  // Maximum timeline tick for Gantt width
  const maxTick = Math.max(m.makespan_ticks, 400);

  return (
    <div style={{ minHeight: '100vh', display: 'flex', flexDirection: 'column', backgroundColor: '#0b1120', color: '#f8fafc' }}>
      {/* Top Header */}
      <header style={{ borderBottom: '1px solid #1e293b', padding: '16px 24px', backgroundColor: '#0f172a', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ backgroundColor: '#10b981', color: '#042f2e', fontWeight: 800, padding: '4px 10px', borderRadius: '4px', fontSize: '13px', letterSpacing: '0.05em' }}>
            HETEROSCHED
          </div>
          <span style={{ fontSize: '14px', color: '#94a3b8' }}>Deterministic Heterogeneous Workload Scheduler</span>
          <span style={{ backgroundColor: '#38bdf820', color: '#38bdf8', border: '1px solid #38bdf840', padding: '2px 8px', borderRadius: '12px', fontSize: '11px', fontWeight: 600 }}>
            SIMULATED VIRTUAL TIME
          </span>
        </div>
        <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
          <div style={{ fontSize: '12px', color: '#cbd5e1', backgroundColor: '#1e293b', padding: '6px 12px', borderRadius: '6px' }}>
            Workload: <strong style={{ color: '#38bdf8' }}>{sim.workload_file.split(/[\/\\]/).pop()}</strong>
          </div>
          <div style={{ fontSize: '12px', color: '#cbd5e1', backgroundColor: '#1e293b', padding: '6px 12px', borderRadius: '6px' }}>
            Active Policy: <strong style={{ color: '#34d399' }}>{sim.policy.toUpperCase()}</strong>
          </div>
        </div>
      </header>

      {/* KPI Stats Bar */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))', gap: '12px', padding: '16px 24px', backgroundColor: '#0f172a', borderBottom: '1px solid #1e293b' }}>
        <div style={{ backgroundColor: '#1e293b', padding: '12px 16px', borderRadius: '8px', borderLeft: '4px solid #38bdf8' }}>
          <div style={{ fontSize: '11px', color: '#94a3b8', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Makespan</div>
          <div style={{ fontSize: '20px', fontWeight: 700, color: '#f8fafc', marginTop: '4px' }}>
            {m.makespan_ticks} <span style={{ fontSize: '12px', color: '#94a3b8' }}>μs</span>
          </div>
        </div>
        <div style={{ backgroundColor: '#1e293b', padding: '12px 16px', borderRadius: '8px', borderLeft: '4px solid #34d399' }}>
          <div style={{ fontSize: '11px', color: '#94a3b8', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Throughput</div>
          <div style={{ fontSize: '20px', fontWeight: 700, color: '#f8fafc', marginTop: '4px' }}>
            {m.throughput_tps.toLocaleString()} <span style={{ fontSize: '12px', color: '#94a3b8' }}>tasks/s</span>
          </div>
        </div>
        <div style={{ backgroundColor: '#1e293b', padding: '12px 16px', borderRadius: '8px', borderLeft: '4px solid #c084fc' }}>
          <div style={{ fontSize: '11px', color: '#94a3b8', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Latency (p50 / p95)</div>
          <div style={{ fontSize: '18px', fontWeight: 700, color: '#f8fafc', marginTop: '4px' }}>
            {m.latency_p50_ticks} / {m.latency_p95_ticks} <span style={{ fontSize: '12px', color: '#94a3b8' }}>μs</span>
          </div>
        </div>
        <div style={{ backgroundColor: '#1e293b', padding: '12px 16px', borderRadius: '8px', borderLeft: '4px solid #fbbf24' }}>
          <div style={{ fontSize: '11px', color: '#94a3b8', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Slot Util (CPU / GPU)</div>
          <div style={{ fontSize: '18px', fontWeight: 700, color: '#f8fafc', marginTop: '4px' }}>
            {m.cpu_utilization_pct.toFixed(1)}% / {m.gpu_utilization_pct.toFixed(1)}%
          </div>
        </div>
        <div style={{ backgroundColor: '#1e293b', padding: '12px 16px', borderRadius: '8px', borderLeft: '4px solid #f87171' }}>
          <div style={{ fontSize: '11px', color: '#94a3b8', textTransform: 'uppercase', letterSpacing: '0.05em' }}>DMA Engine Util</div>
          <div style={{ fontSize: '20px', fontWeight: 700, color: '#f8fafc', marginTop: '4px' }}>
            {m.dma_utilization_pct.toFixed(1)}%
          </div>
        </div>
      </div>

      {/* Navigation Tabs */}
      <div style={{ display: 'flex', gap: '8px', padding: '12px 24px', backgroundColor: '#0b1120', borderBottom: '1px solid #1e293b' }}>
        <button
          onClick={() => setActiveTab('timeline')}
          style={{ padding: '8px 16px', borderRadius: '6px', border: 'none', cursor: 'pointer', fontWeight: 600, fontSize: '13px', backgroundColor: activeTab === 'timeline' ? '#38bdf8' : '#1e293b', color: activeTab === 'timeline' ? '#0f172a' : '#cbd5e1' }}
        >
          Execution Timeline (Gantt)
        </button>
        <button
          onClick={() => setActiveTab('tasks')}
          style={{ padding: '8px 16px', borderRadius: '6px', border: 'none', cursor: 'pointer', fontWeight: 600, fontSize: '13px', backgroundColor: activeTab === 'tasks' ? '#38bdf8' : '#1e293b', color: activeTab === 'tasks' ? '#0f172a' : '#cbd5e1' }}
        >
          Tasks & DAG View ({sim.tasks.length})
        </button>
        <button
          onClick={() => setActiveTab('comparison')}
          style={{ padding: '8px 16px', borderRadius: '6px', border: 'none', cursor: 'pointer', fontWeight: 600, fontSize: '13px', backgroundColor: activeTab === 'comparison' ? '#38bdf8' : '#1e293b', color: activeTab === 'comparison' ? '#0f172a' : '#cbd5e1' }}
        >
          Policy Benchmarks
        </button>
        <button
          onClick={() => setActiveTab('events')}
          style={{ padding: '8px 16px', borderRadius: '6px', border: 'none', cursor: 'pointer', fontWeight: 600, fontSize: '13px', backgroundColor: activeTab === 'events' ? '#38bdf8' : '#1e293b', color: activeTab === 'events' ? '#0f172a' : '#cbd5e1' }}
        >
          Canonical Event Inspector ({m.canonical_events.length})
        </button>
      </div>

      {/* Main Content Area */}
      <main style={{ flex: 1, padding: '24px', overflowY: 'auto' }}>
        {/* TAB 1: TIMELINE VIEW */}
        {activeTab === 'timeline' && (
          <div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
              <div>
                <h2 style={{ fontSize: '16px', fontWeight: 700, color: '#f8fafc' }}>Heterogeneous Resource Swimlanes</h2>
                <p style={{ fontSize: '12px', color: '#94a3b8' }}>Horizontal execution blocks on CPU worker slots, GPU stream slots, and PCIe DMA copy engines.</p>
              </div>
              <div style={{ display: 'flex', gap: '16px', fontSize: '12px' }}>
                <span style={{ display: 'flex', alignItems: 'center', gap: '6px' }}><div style={{ width: '12px', height: '12px', backgroundColor: '#38bdf8', borderRadius: '2px' }}></div> CPU Compute</span>
                <span style={{ display: 'flex', alignItems: 'center', gap: '6px' }}><div style={{ width: '12px', height: '12px', backgroundColor: '#34d399', borderRadius: '2px' }}></div> GPU Compute</span>
                <span style={{ display: 'flex', alignItems: 'center', gap: '6px' }}><div style={{ width: '12px', height: '12px', backgroundColor: '#fbbf24', borderRadius: '2px' }}></div> PCIe DMA Transfer</span>
              </div>
            </div>

            {/* Gantt Chart Container */}
            <div style={{ backgroundColor: '#0f172a', border: '1px solid #1e293b', borderRadius: '8px', padding: '20px', overflowX: 'auto' }}>
              {/* CPU Slots */}
              {Array.from({ length: cfg.cpu_slots }).map((_, slotIdx) => (
                <div key={`cpu-${slotIdx}`} style={{ display: 'flex', alignItems: 'center', marginBottom: '12px' }}>
                  <div style={{ width: '130px', fontSize: '12px', color: '#94a3b8', fontFamily: 'monospace' }}>CPU Worker {slotIdx}</div>
                  <div style={{ flex: 1, height: '32px', backgroundColor: '#1e293b', borderRadius: '4px', position: 'relative' }}>
                    {sim.tasks
                      .filter(t => t.assigned_device === 'CPU' && t.assigned_slot === slotIdx && t.finish_tick > t.compute_start_tick)
                      .map(t => {
                        const leftPct = (t.compute_start_tick / maxTick) * 100;
                        const widthPct = Math.max(((t.finish_tick - t.compute_start_tick) / maxTick) * 100, 1.5);
                        return (
                          <div
                            key={t.id}
                            onClick={() => setSelectedTask(t)}
                            title={`${t.name} (T${t.id}) [${t.compute_start_tick} - ${t.finish_tick} us]`}
                            style={{
                              position: 'absolute',
                              left: `${leftPct}%`,
                              width: `${widthPct}%`,
                              height: '26px',
                              top: '3px',
                              backgroundColor: '#38bdf8',
                              color: '#0f172a',
                              borderRadius: '4px',
                              fontSize: '11px',
                              fontWeight: 700,
                              display: 'flex',
                              alignItems: 'center',
                              justifyContent: 'center',
                              cursor: 'pointer',
                              overflow: 'hidden',
                              whiteSpace: 'nowrap',
                              textOverflow: 'ellipsis',
                              padding: '0 4px',
                              border: selectedTask?.id === t.id ? '2px solid #ffffff' : 'none'
                            }}
                          >
                            T{t.id}: {t.name}
                          </div>
                        );
                      })}
                  </div>
                </div>
              ))}

              {/* GPU Stream Slots */}
              {Array.from({ length: cfg.gpu_slots }).map((_, slotIdx) => (
                <div key={`gpu-${slotIdx}`} style={{ display: 'flex', alignItems: 'center', marginBottom: '12px' }}>
                  <div style={{ width: '130px', fontSize: '12px', color: '#34d399', fontFamily: 'monospace' }}>GPU Stream {slotIdx}</div>
                  <div style={{ flex: 1, height: '32px', backgroundColor: '#1e293b', borderRadius: '4px', position: 'relative' }}>
                    {sim.tasks
                      .filter(t => t.assigned_device === 'GPU' && t.assigned_slot === slotIdx && t.finish_tick > t.compute_start_tick)
                      .map(t => {
                        const leftPct = (t.compute_start_tick / maxTick) * 100;
                        const widthPct = Math.max(((t.finish_tick - t.compute_start_tick) / maxTick) * 100, 1.5);
                        return (
                          <div
                            key={t.id}
                            onClick={() => setSelectedTask(t)}
                            title={`${t.name} (T${t.id}) [${t.compute_start_tick} - ${t.finish_tick} us]`}
                            style={{
                              position: 'absolute',
                              left: `${leftPct}%`,
                              width: `${widthPct}%`,
                              height: '26px',
                              top: '3px',
                              backgroundColor: '#34d399',
                              color: '#042f2e',
                              borderRadius: '4px',
                              fontSize: '11px',
                              fontWeight: 700,
                              display: 'flex',
                              alignItems: 'center',
                              justifyContent: 'center',
                              cursor: 'pointer',
                              overflow: 'hidden',
                              whiteSpace: 'nowrap',
                              textOverflow: 'ellipsis',
                              padding: '0 4px',
                              border: selectedTask?.id === t.id ? '2px solid #ffffff' : 'none'
                            }}
                          >
                            T{t.id}: {t.name}
                          </div>
                        );
                      })}
                  </div>
                </div>
              ))}

              {/* DMA Engines */}
              {Array.from({ length: cfg.dma_engines }).map((_, dmaIdx) => (
                <div key={`dma-${dmaIdx}`} style={{ display: 'flex', alignItems: 'center', marginBottom: '8px' }}>
                  <div style={{ width: '130px', fontSize: '12px', color: '#fbbf24', fontFamily: 'monospace' }}>DMA Engine {dmaIdx}</div>
                  <div style={{ flex: 1, height: '32px', backgroundColor: '#1e293b', borderRadius: '4px', position: 'relative' }}>
                    {sim.tasks
                      .filter(t => t.transfer_end_tick > t.transfer_start_tick && t.assigned_dma === dmaIdx)
                      .map(t => {
                        const leftPct = (t.transfer_start_tick / maxTick) * 100;
                        const widthPct = Math.max(((t.transfer_end_tick - t.transfer_start_tick) / maxTick) * 100, 1.5);
                        return (
                          <div
                            key={`dma-${t.id}`}
                            onClick={() => setSelectedTask(t)}
                            title={`DMA Transfer: ${t.name} [${t.transfer_start_tick} - ${t.transfer_end_tick} us]`}
                            style={{
                              position: 'absolute',
                              left: `${leftPct}%`,
                              width: `${widthPct}%`,
                              height: '26px',
                              top: '3px',
                              backgroundColor: '#fbbf24',
                              color: '#451a03',
                              borderRadius: '4px',
                              fontSize: '11px',
                              fontWeight: 700,
                              display: 'flex',
                              alignItems: 'center',
                              justifyContent: 'center',
                              cursor: 'pointer',
                              overflow: 'hidden',
                              whiteSpace: 'nowrap',
                              textOverflow: 'ellipsis',
                              padding: '0 4px',
                              border: selectedTask?.id === t.id ? '2px solid #ffffff' : 'none'
                            }}
                          >
                            DMA: T{t.id}
                          </div>
                        );
                      })}
                  </div>
                </div>
              ))}
            </div>

            {/* Selected Task Inspector */}
            {selectedTask && (
              <div style={{ marginTop: '20px', backgroundColor: '#1e293b', border: '1px solid #334155', borderRadius: '8px', padding: '16px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', borderBottom: '1px solid #334155', paddingBottom: '8px', marginBottom: '12px' }}>
                  <h3 style={{ fontSize: '14px', fontWeight: 700, color: '#38bdf8' }}>
                    Inspecting Task #{selectedTask.id}: {selectedTask.name}
                  </h3>
                  <button onClick={() => setSelectedTask(null)} style={{ background: 'none', border: 'none', color: '#94a3b8', cursor: 'pointer', fontSize: '14px' }}>✕</button>
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px', fontSize: '12px' }}>
                  <div><span style={{ color: '#94a3b8' }}>Status:</span> <strong>{selectedTask.state}</strong></div>
                  <div><span style={{ color: '#94a3b8' }}>Assigned Device:</span> <strong>{selectedTask.assigned_device}</strong> (Slot {selectedTask.assigned_slot})</div>
                  <div><span style={{ color: '#94a3b8' }}>Transfer Window:</span> {selectedTask.transfer_start_tick} - {selectedTask.transfer_end_tick} μs</div>
                  <div><span style={{ color: '#94a3b8' }}>Compute Window:</span> {selectedTask.compute_start_tick} - {selectedTask.finish_tick} μs</div>
                  <div><span style={{ color: '#94a3b8' }}>Input Bytes:</span> {(selectedTask.input_bytes / 1024).toFixed(1)} KB</div>
                  <div><span style={{ color: '#94a3b8' }}>Working Bytes:</span> {(selectedTask.working_bytes / 1024).toFixed(1)} KB</div>
                  <div><span style={{ color: '#94a3b8' }}>Output Bytes:</span> {(selectedTask.output_bytes / 1024).toFixed(1)} KB</div>
                  <div><span style={{ color: '#94a3b8' }}>Dependencies:</span> {selectedTask.dependencies.join(', ') || 'None (Root)'}</div>
                </div>
              </div>
            )}
          </div>
        )}

        {/* TAB 2: TASKS & DAG TABLE */}
        {activeTab === 'tasks' && (
          <div>
            <h2 style={{ fontSize: '16px', fontWeight: 700, marginBottom: '16px' }}>Task Execution & Dependency Records</h2>
            <div style={{ backgroundColor: '#0f172a', border: '1px solid #1e293b', borderRadius: '8px', overflow: 'hidden' }}>
              <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '12px', textAlign: 'left' }}>
                <thead>
                  <tr style={{ backgroundColor: '#1e293b', color: '#94a3b8', borderBottom: '1px solid #334155' }}>
                    <th style={{ padding: '10px 14px' }}>ID</th>
                    <th style={{ padding: '10px 14px' }}>Name</th>
                    <th style={{ padding: '10px 14px' }}>State</th>
                    <th style={{ padding: '10px 14px' }}>Device</th>
                    <th style={{ padding: '10px 14px' }}>Start - Finish (μs)</th>
                    <th style={{ padding: '10px 14px' }}>Duration</th>
                    <th style={{ padding: '10px 14px' }}>Input Size</th>
                    <th style={{ padding: '10px 14px' }}>Dependencies</th>
                  </tr>
                </thead>
                <tbody>
                  {sim.tasks.map(t => (
                    <tr key={t.id} style={{ borderBottom: '1px solid #1e293b' }}>
                      <td style={{ padding: '10px 14px', fontWeight: 600 }}>T{t.id}</td>
                      <td style={{ padding: '10px 14px' }}>{t.name}</td>
                      <td style={{ padding: '10px 14px' }}>
                        <span style={{ padding: '2px 8px', borderRadius: '4px', fontSize: '11px', fontWeight: 600, backgroundColor: t.state === 'COMPLETED' ? '#042f2e' : '#450a0a', color: t.state === 'COMPLETED' ? '#34d399' : '#f87171' }}>
                          {t.state}
                        </span>
                      </td>
                      <td style={{ padding: '10px 14px' }}>
                        <span style={{ color: t.assigned_device === 'GPU' ? '#34d399' : '#38bdf8', fontWeight: 600 }}>
                          {t.assigned_device}
                        </span>
                      </td>
                      <td style={{ padding: '10px 14px', fontFamily: 'monospace' }}>{t.compute_start_tick} - {t.finish_tick}</td>
                      <td style={{ padding: '10px 14px' }}>{t.finish_tick - t.compute_start_tick} μs</td>
                      <td style={{ padding: '10px 14px' }}>{(t.input_bytes / 1024).toFixed(0)} KB</td>
                      <td style={{ padding: '10px 14px', color: '#94a3b8' }}>
                        {t.dependencies.length > 0 ? t.dependencies.map(d => `T${d}`).join(', ') : 'None'}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        )}

        {/* TAB 3: BENCHMARK COMPARISONS */}
        {activeTab === 'comparison' && (
          <div>
            <h2 style={{ fontSize: '16px', fontWeight: 700, marginBottom: '8px' }}>Empirical Policy Comparison Across Workload Scenarios</h2>
            <p style={{ fontSize: '12px', color: '#94a3b8', marginBottom: '16px' }}>Measured simulated results across identical workload seeds and configurations.</p>
            <div style={{ backgroundColor: '#0f172a', border: '1px solid #1e293b', borderRadius: '8px', overflow: 'hidden' }}>
              <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '12px', textAlign: 'left' }}>
                <thead>
                  <tr style={{ backgroundColor: '#1e293b', color: '#94a3b8', borderBottom: '1px solid #334155' }}>
                    <th style={{ padding: '10px 14px' }}>Workload</th>
                    <th style={{ padding: '10px 14px' }}>Policy</th>
                    <th style={{ padding: '10px 14px' }}>Makespan (μs)</th>
                    <th style={{ padding: '10px 14px' }}>Throughput (tasks/s)</th>
                    <th style={{ padding: '10px 14px' }}>Deadline Miss %</th>
                    <th style={{ padding: '10px 14px' }}>GPU Util %</th>
                    <th style={{ padding: '10px 14px' }}>DMA Util %</th>
                  </tr>
                </thead>
                <tbody>
                  {benchmarkSummary.map((b: BenchmarkRecord, idx: number) => (
                    <tr key={idx} style={{ borderBottom: '1px solid #1e293b' }}>
                      <td style={{ padding: '10px 14px', fontWeight: 600, color: '#f8fafc' }}>{b.workload}</td>
                      <td style={{ padding: '10px 14px' }}>
                        <span style={{ fontWeight: 600, color: b.policy === 'heft' || b.policy === 'eft' ? '#38bdf8' : '#cbd5e1' }}>
                          {b.policy}
                        </span>
                      </td>
                      <td style={{ padding: '10px 14px', fontFamily: 'monospace' }}>{b.makespan_ticks}</td>
                      <td style={{ padding: '10px 14px', fontFamily: 'monospace' }}>{b.throughput_tps.toLocaleString()}</td>
                      <td style={{ padding: '10px 14px', color: b.deadline_miss_ratio > 0 ? '#f87171' : '#34d399' }}>{b.deadline_miss_ratio}%</td>
                      <td style={{ padding: '10px 14px' }}>{b.gpu_util_pct}%</td>
                      <td style={{ padding: '10px 14px' }}>{b.dma_util_pct}%</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        )}

        {/* TAB 4: CANONICAL EVENT INSPECTOR */}
        {activeTab === 'events' && (
          <div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
              <h2 style={{ fontSize: '16px', fontWeight: 700 }}>Canonical Discrete-Event Log (Single Source of Truth)</h2>
              <input
                type="text"
                placeholder="Filter events (e.g. DISPATCH, TRANSFER, T1)..."
                value={eventFilter}
                onChange={e => setEventFilter(e.target.value)}
                style={{ backgroundColor: '#1e293b', border: '1px solid #334155', borderRadius: '6px', color: '#f8fafc', padding: '6px 12px', fontSize: '12px', width: '280px' }}
              />
            </div>
            <div style={{ backgroundColor: '#0f172a', border: '1px solid #1e293b', borderRadius: '8px', overflow: 'hidden' }}>
              <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '12px', textAlign: 'left', fontFamily: 'monospace' }}>
                <thead>
                  <tr style={{ backgroundColor: '#1e293b', color: '#94a3b8', borderBottom: '1px solid #334155' }}>
                    <th style={{ padding: '8px 12px' }}>Tick (μs)</th>
                    <th style={{ padding: '8px 12px' }}>Phase</th>
                    <th style={{ padding: '8px 12px' }}>Event Type</th>
                    <th style={{ padding: '8px 12px' }}>Task</th>
                    <th style={{ padding: '8px 12px' }}>Device</th>
                    <th style={{ padding: '8px 12px' }}>Metadata / Action Diagnostics</th>
                  </tr>
                </thead>
                <tbody>
                  {m.canonical_events
                    .filter((ev: CanonicalEvent) => {
                      if (!eventFilter) return true;
                      const q = eventFilter.toLowerCase();
                      return ev.type.toLowerCase().includes(q) || ev.metadata.toLowerCase().includes(q) || `t${ev.task_id}`.includes(q);
                    })
                    .slice(0, 150)
                    .map((ev: CanonicalEvent, idx: number) => (
                      <tr key={idx} style={{ borderBottom: '1px solid #1e293b' }}>
                        <td style={{ padding: '6px 12px', color: '#38bdf8' }}>{ev.tick}</td>
                        <td style={{ padding: '6px 12px', color: '#94a3b8' }}>P{ev.phase}</td>
                        <td style={{ padding: '6px 12px', color: '#34d399', fontWeight: 600 }}>{ev.type}</td>
                        <td style={{ padding: '6px 12px' }}>T{ev.task_id}</td>
                        <td style={{ padding: '6px 12px' }}>{ev.device}</td>
                        <td style={{ padding: '6px 12px', color: '#cbd5e1' }}>{ev.metadata}</td>
                      </tr>
                    ))}
                </tbody>
              </table>
            </div>
          </div>
        )}
      </main>
    </div>
  );
}
