import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from matplotlib.gridspec import GridSpec

def load_metrics():
    metrics = pd.read_csv('metrics.csv')
    metrics = metrics.set_index('metric')['value'].to_dict()
    
    worker_data = [
        ('Word Count', metrics.get('word_count_total', 0)),
        ('Top Words', metrics.get('top_words_total', 0)),
        ('Longest Word', metrics.get('longest_word_total', 0))
    ]
    
    return metrics, pd.DataFrame(worker_data, columns=['Worker', 'Total Time (ms)'])

def create_enhanced_visualizations():
    try:
        # Set the aesthetic style of the plots
        # sns.set_style("whitegrid")
        # plt.style.use('seaborn')
        plt.style.use('seaborn-v0_8-whitegrid')

        # Create figure with custom grid layout
        fig = plt.figure(figsize=(18, 12), facecolor='#f5f5f5')
        fig.suptitle('WEB CRAWLER PERFORMANCE METRICS', 
                    fontsize=24, fontweight='bold', y=0.98)
        
        gs = GridSpec(3, 3, figure=fig, hspace=0.4, wspace=0.3)
        
        metrics, worker_df = load_metrics()
        
        # Color palette
        colors = sns.color_palette("husl", 8)
        
        # 1. System Time Breakdown (Top Left)
        ax1 = fig.add_subplot(gs[0, 0])
        time_metrics = {
            'URL Download': metrics['url_download'],
            'Thread Dist.': metrics['thread_distribution'],
            'Processing': metrics['thread_execution']
        }
        time_df = pd.DataFrame(time_metrics.items(), columns=['Phase', 'Time (ms)'])
        
        bar1 = sns.barplot(x='Phase', y='Time (ms)', data=time_df, hue='Phase',
                          palette=colors[:3], ax=ax1, legend=False)
        ax1.set_title('SYSTEM TIME BREAKDOWN', fontsize=14, fontweight='bold', pad=15)
        ax1.set_ylabel('Time (ms)', fontsize=12)
        ax1.set_xlabel('')
        plt.setp(ax1.get_xticklabels(), rotation=15)
        
        # Add value labels on bars
        for p in bar1.patches:
            bar1.annotate(f"{p.get_height():.1f} ms", 
                         (p.get_x() + p.get_width() / 2., p.get_height()),
                         ha='center', va='center', 
                         xytext=(0, 9), 
                         textcoords='offset points',
                         fontsize=10)
        
        # 2. Worker Efficiency (Top Right)
        ax2 = fig.add_subplot(gs[0, 1:])
        worker_df['Avg Time (ms)'] = worker_df['Total Time (ms)'] / metrics['urls_processed']
        
        bar2 = sns.barplot(x='Worker', y='Total Time (ms)', data=worker_df, hue='Worker', palette=colors[3:6], ax=ax2, legend=False)

        ax2.set_title('WORKER PERFORMANCE', fontsize=14, fontweight='bold', pad=15)
        ax2.set_ylabel('Total Time (ms)', fontsize=12)
        ax2.set_xlabel('Worker Type', fontsize=12)
        
        # Add value labels on bars
        for p in bar2.patches:
            bar2.annotate(f"{p.get_height():.1f} ms", 
                         (p.get_x() + p.get_width() / 2., p.get_height()),
                         ha='center', va='center', 
                         xytext=(0, 9), 
                         textcoords='offset points',
                         fontsize=10)
        
        # 3. Time Distribution (Middle Row)
        ax3 = fig.add_subplot(gs[1, :])
        labels = ['Download', 'Processing', 'Other']
        sizes = [
            metrics['url_download'],
            metrics['thread_execution'],
            metrics['thread_distribution']
        ]
        explode = (0.1, 0, 0)  # emphasize the download portion
        
        wedges, texts, autotexts = ax3.pie(
            sizes, explode=explode, labels=labels, 
            autopct='%1.1f%%', startangle=90,
            colors=colors[:3], shadow=True,
            textprops={'fontsize': 12},
            wedgeprops={'edgecolor': 'white', 'linewidth': 1}
        )
        
        # Make the percentages white and bold
        for autotext in autotexts:
            autotext.set_color('white')
            autotext.set_fontweight('bold')
        
        ax3.set_title('TIME DISTRIBUTION', fontsize=14, fontweight='bold', pad=20)
        
        # 4. Data Throughput (Bottom Left)
        ax4 = fig.add_subplot(gs[2, 0])
        data_metrics = {
            'Processed': metrics['urls_processed'],
            'Failed': metrics['urls_failed'],
            'Data (KB)': metrics['total_bytes'] / 1024
        }
        data_df = pd.DataFrame(data_metrics.items(), columns=['Metric', 'Value'])
        
        bar4 = sns.barplot(x='Metric', y='Value', data=data_df, hue='Metric',
                          palette=colors[5:8], ax=ax4, legend=False)
        ax4.set_title('DATA THROUGHPUT', fontsize=14, fontweight='bold', pad=15)
        ax4.set_ylabel('Count / KB', fontsize=12)
        ax4.set_xlabel('')
        
        # Add value labels on bars
        for p in bar4.patches:
            bar4.annotate(f"{p.get_height():.1f}", 
                         (p.get_x() + p.get_width() / 2., p.get_height()),
                         ha='center', va='center', 
                         xytext=(0, 9), 
                         textcoords='offset points',
                         fontsize=10)
        
        # 5. Summary Statistics (Bottom Right)
        ax5 = fig.add_subplot(gs[2, 1:])
        ax5.axis('off')
        
        summary_text = [
            f"Total Runtime: {metrics['total_runtime']:.1f} ms",
            f"URLs Processed: {metrics['urls_processed']}",
            f"Data Downloaded: {metrics['total_bytes']/1024:.1f} KB",
            f"Word Count Tasks: {worker_df.loc[0, 'Total Time (ms)']:.1f} ms",
            f"Top Words Tasks: {worker_df.loc[1, 'Total Time (ms)']:.1f} ms",
            f"Longest Word Tasks: {worker_df.loc[2, 'Total Time (ms)']:.1f} ms"
        ]
        
        ax5.text(0.1, 0.9, "PERFORMANCE SUMMARY", 
                fontsize=16, fontweight='bold', color=colors[0])
        
        for i, text in enumerate(summary_text):
            ax5.text(0.1, 0.7 - i*0.12, text, 
                    fontsize=14, color=colors[i+1])
        
        # Add decorative elements
        fig.text(0.5, 0.02, f"Generated on {pd.Timestamp.now().strftime('%Y-%m-%d %H:%M')}",
                ha='center', fontsize=10, alpha=0.7)
        
        # Save and show
        plt.savefig('enhanced_performance_metrics.png', 
                   dpi=300, bbox_inches='tight', facecolor=fig.get_facecolor())
        print("Enhanced visualization saved as 'enhanced_performance_metrics.png'")
        plt.show()
        
    except Exception as e:
        print(f"Error generating visualizations: {str(e)}")

if __name__ == "__main__":
    create_enhanced_visualizations()