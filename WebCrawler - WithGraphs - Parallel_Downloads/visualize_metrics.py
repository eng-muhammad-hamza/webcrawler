import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from matplotlib.gridspec import GridSpec

def load_metrics():
    # Read the metrics.csv file into a pandas DataFrame
    metrics = pd.read_csv('metrics.csv')
    # Set the 'metric' column as the index and convert to a dictionary of metric:value pairs
    metrics = metrics.set_index('metric')['value'].to_dict()
    
    # Prepare a list of tuples for each worker metric and its total time (ms)
    worker_data = [
        ('Word Count', metrics.get('word_count_total', 0)),             # Total time for word count worker
        ('Top Words', metrics.get('top_words_total', 0)),               # Total time for top words worker
        ('Longest Word', metrics.get('longest_word_total', 0)),         # Total time for longest word worker
        ('Sentence Count', metrics.get('sentence_count_total', 0)),     # Total time for sentence count worker
        ('Char Frequency', metrics.get('char_freq_total', 0)),          # Total time for character frequency worker
        ('Palindromes', metrics.get('palindrome_total', 0)),            # Total time for palindrome worker
        ('Word Length Dist', metrics.get('word_len_dist_total', 0)),    # Total time for word length distribution worker
        ('Word Start Char', metrics.get('word_start_char_total', 0)),   # Total time for word start character worker
        ('Avg Word Length', metrics.get('avg_word_len_total', 0))       # Total time for average word length worker
    ]
    
    # Return the metrics dictionary and a DataFrame of worker data
    return metrics, pd.DataFrame(worker_data, columns=['Worker', 'Total Time (ms)'])

def create_enhanced_visualizations():
    try:
        # Set the style for all matplotlib plots to a seaborn theme
        plt.style.use('seaborn-v0_8-whitegrid')
        # Create a new figure for the dashboard with a specific size and background color
        fig = plt.figure(figsize=(20, 16), facecolor='#f5f5f5')
        # Set the main title for the dashboard
        fig.suptitle('WEB CRAWLER PERFORMANCE METRICS (ENHANCED)', 
                    fontsize=24, fontweight='bold', y=0.98)
        
        # Create a grid layout for subplots: 4 rows, 3 columns, with spacing
        gs = GridSpec(4, 3, figure=fig, hspace=0.5, wspace=0.4)
        # Load metrics and worker data
        metrics, worker_df = load_metrics()
        # Get a color palette for the plots
        colors = sns.color_palette("husl", 10)
        
        # 1. SYSTEM TIME BREAKDOWN BAR CHART
        ax1 = fig.add_subplot(gs[0, 0])                                 # Place in first cell of grid
        # Prepare a dictionary of system time metrics
        time_metrics = {
            'URL Download': metrics['url_download'],                    # Time spent downloading URLs
            'Thread Dist.': metrics['thread_distribution'],             # Time spent distributing threads
            'Processing': metrics['thread_execution']                   # Time spent processing
        }
        # Convert to DataFrame for plotting
        time_df = pd.DataFrame(time_metrics.items(), columns=['Phase', 'Time (ms)'])
        # Create a barplot for system time breakdown
        sns.barplot(x='Phase', y='Time (ms)', data=time_df, hue='Phase', palette=colors[:3], ax=ax1)
        # Set the title for this subplot
        ax1.set_title('SYSTEM TIME BREAKDOWN', fontsize=14, fontweight='bold')
        # Annotate each bar with its value
        for p in ax1.patches:
            ax1.annotate(f"{p.get_height():.1f} ms", 
                        (p.get_x() + p.get_width() / 2., p.get_height()),
                        ha='center', va='center', xytext=(0, 9), 
                        textcoords='offset points', fontsize=10)

        # 2. WORKER PERFORMANCE COMPARISON BAR CHART
        ax2 = fig.add_subplot(gs[0, 1:])  # Place in first row, columns 2 and 3
        # Sort workers by total time descending
        worker_df = worker_df.sort_values('Total Time (ms)', ascending=False)
        # Create a barplot for worker performance
        sns.barplot(x='Worker', y='Total Time (ms)', data=worker_df, hue='Worker',
                   palette=colors[1:10], ax=ax2)
        # Set the title for this subplot
        ax2.set_title('WORKER PERFORMANCE COMPARISON', fontsize=14, fontweight='bold')
        # Rotate x-axis labels for readability
        ax2.tick_params(axis='x', rotation=45)
        # Annotate each bar with its value
        for p in ax2.patches:
            ax2.annotate(f"{p.get_height():.1f} ms", 
                        (p.get_x() + p.get_width() / 2., p.get_height()),
                        ha='center', va='center', xytext=(0, 9), 
                        textcoords='offset points', fontsize=9)

        # 3. TIME DISTRIBUTION PIE CHART
        ax3 = fig.add_subplot(gs[1, 0])  # Place in second row, first column
        # Prepare data for the pie chart
        time_pie_data = {
            'Download': metrics['url_download'],
            'Processing': metrics['thread_execution'],
            'Distribution': metrics['thread_distribution']
        }
        # Create a pie chart for time distribution
        ax3.pie(time_pie_data.values(), labels=time_pie_data.keys(), 
               autopct='%1.1f%%', startangle=90, colors=colors[:3],
               wedgeprops={'edgecolor': 'white', 'linewidth': 1})
        # Set the title for this subplot
        ax3.set_title('TIME DISTRIBUTION', fontsize=14, fontweight='bold')

        # 4. WORKER TIME DISTRIBUTION PIE CHART
        ax4 = fig.add_subplot(gs[1, 1:])  # Place in second row, columns 2 and 3
        # Prepare worker time data for pie chart
        worker_time_data = worker_df.set_index('Worker')['Total Time (ms)']
        # Plot worker time distribution as a pie chart
        worker_time_data.plot.pie(ax=ax4, autopct='%1.1f%%', startangle=90,
                                colors=colors[3:], 
                                wedgeprops={'edgecolor': 'white', 'linewidth': 1})
        # Remove y-axis label
        ax4.set_ylabel('')
        # Set the title for this subplot
        ax4.set_title('WORKER TIME DISTRIBUTION', fontsize=14, fontweight='bold')

        # 5. DATA THROUGHPUT BAR CHART
        ax5 = fig.add_subplot(gs[2, 0])  # Place in third row, first column
        # Prepare data throughput metrics
        throughput_data = {
            'Processed': metrics['urls_processed'],                     # Number of URLs processed
            'Failed': metrics['urls_failed'],                           # Number of URLs failed
            'Data (MB)': metrics['total_bytes'] / (1024*1024)           # Total data downloaded in MB
        }
        # Convert to DataFrame for plotting
        throughput_df = pd.DataFrame(throughput_data.items(), columns=['Metric', 'Value'])
        # Create a barplot for data throughput
        sns.barplot(x='Metric', y='Value', data=throughput_df, hue='Metric', palette=colors[6:9], ax=ax5)
        # Set the title for this subplot
        ax5.set_title('DATA THROUGHPUT', fontsize=14, fontweight='bold')
        # Annotate each bar with its value
        for p in ax5.patches:
            ax5.annotate(f"{p.get_height():.1f}", 
                        (p.get_x() + p.get_width() / 2., p.get_height()),
                        ha='center', va='center', xytext=(0, 9), 
                        textcoords='offset points', fontsize=10)

        # 6. WORKER EFFICIENCY (TIME PER URL) BAR CHART
        ax6 = fig.add_subplot(gs[2, 1:])  # Place in third row, columns 2 and 3
        # Calculate time per URL for each worker
        worker_df['Time per URL'] = worker_df['Total Time (ms)'] / metrics['urls_processed']
        # Sort workers by time per URL descending
        worker_df = worker_df.sort_values('Time per URL', ascending=False)
        # Create a barplot for worker efficiency
        sns.barplot(x='Worker', y='Time per URL', data=worker_df, hue='Worker',palette=colors[1:], ax=ax6)
        # Set the title for this subplot
        ax6.set_title('WORKER EFFICIENCY (TIME PER URL)', fontsize=14, fontweight='bold')
        # Rotate x-axis labels for readability
        ax6.tick_params(axis='x', rotation=45)
        # Annotate each bar with its value
        for p in ax6.patches:
            ax6.annotate(f"{p.get_height():.2f} ms", 
                        (p.get_x() + p.get_width() / 2., p.get_height()),
                        ha='center', va='center', xytext=(0, 9), 
                        textcoords='offset points', fontsize=9)

        # 7. SUMMARY STATISTICS TEXT BOX
        ax7 = fig.add_subplot(gs[3, :])  # Place in fourth row, all columns
        # Hide the axis for this subplot
        ax7.axis('off')
        # Prepare summary statistics as a list of strings
        summary_text = [
            f"Total Runtime: {metrics['total_runtime']:.1f} ms",    # Total runtime of the crawler
            f"URLs Processed: {metrics['urls_processed']} (Failed: {metrics['urls_failed']})",      # URLs processed and failed
            f"Data Downloaded: {metrics['total_bytes']/(1024*1024):.2f} MB",                        # Total data downloaded in MB
            f"Most Time-Consuming Worker: {worker_df.iloc[0]['Worker']} ({worker_df.iloc[0]['Total Time (ms)']:.1f} ms)",   # Worker with highest total time
            f"Most Efficient Worker: {worker_df.iloc[-1]['Worker']} ({worker_df.iloc[-1]['Time per URL']:.2f} ms/URL)",     # Worker with lowest time per URL
            f"Download Speed: {metrics['total_bytes']/metrics['url_download']:.2f} bytes/ms",       # Download speed in bytes/ms
            f"Processing Speed: {metrics['urls_processed']/(metrics['thread_execution']/1000):.2f} URLs/sec"                # Processing speed in URLs/sec
        ]
        
        # Add a bold heading for the summary
        ax7.text(0.05, 0.9, "PERFORMANCE SUMMARY", fontsize=18, fontweight='bold', color=colors[0])
        # Add each summary line below the heading
        for i, text in enumerate(summary_text):
            ax7.text(0.05, 0.7 - i*0.1, text, fontsize=14, color=colors[i+1])

        # Save the entire dashboard as a PNG image file
        plt.savefig('enhanced_metrics_dashboard.png', dpi=300, bbox_inches='tight')
        # Print a message to the console indicating success
        print("Enhanced visualization saved as 'enhanced_metrics_dashboard.png'")
        # Display the dashboard window
        plt.show()
        
    except Exception as e:
        # If any error occurs, print the error message
        print(f"Error generating visualizations: {str(e)}")

if __name__ == "__main__":
    create_enhanced_visualizations()